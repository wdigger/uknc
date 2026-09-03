// ppuc_load_code.c -- load an RT-11 native relocatable object module
// ("REL": GSD/TXT/RLD/ENDMOD blocks) into PPU memory. The entry point
// is always the returned load address itself -- see below.
//
// Produced directly by this toolchain's own linker, no extra tooling:
//
//   pdp11-uknc-rt11-as foo.s -o foo.o
//   pdp11-uknc-rt11-ld -r -m pdp11rt11rel foo.o -o foo.ppu
//
// This is a small single-module linker, not a generic reader of every
// RT-11 REL file anyone could produce: it only understands the fixed
// .text/.data/.bss/.ABS. p-sect model and the relocation subset that
// ld/emultempl/pdp11rt11rel.em's own emitter ever writes for a single
// module with no unresolved external references -- exactly what a
// self-contained PPU program (now possibly linked from more than one
// object file -- see ppu_server.h/ppus_start.c) compiles to.
//
// PC-relative ("_DISP") entries -- which a cross-function call between
// two merged object files produces, unlike a same-file call the
// assembler already resolves without any relocation at all -- need no
// patch: the operand word already holds the correct, self-relative
// distance between the reference and its same-module target, and
// loading the whole module at a different base shifts both by the
// same amount, which cancels out. So these are only ever parsed far
// enough to skip their operand correctly, never written to. A
// PC-relative reference to a true external symbol (REL_GSD_GLOBAL
// under .ABS., not one of this module's own p-sects -- GLOBAL_DISP/
// GLOBAL_ADD_DISP) is a different, still-unsupported case (EINVAL):
// this loader has no way to resolve an address outside its own
// module.
//
// The object-module format has no "transfer address" field of its own
// (that's only meaningful for a fully linked image); the entry point
// is, by this project's own convention, always .text offset 0 --
// which is why libppu's startup shim (ppus_start.c/ppu_server.h) must
// always be the *first* object file named on the `ld -r` command
// line that produces the module, so its own start() lands there
// regardless of what's linked in after it. No per-module symbol
// lookup is needed to find it.
//
// Only one CPU-side heap buffer is held at once: buf, the whole .PPU
// file, read verbatim. Relocations are patched directly into buf, in
// place, at each TXT block's own original bytes (see chunk_ptr/
// chunk_len in apply_rld_block()/parse_txt_rld() below) -- there is no
// second, separately-allocated image buffer holding the laid-out
// .text/.data/.bss result. Each TXT chunk (now already relocated) is
// written straight from its spot in buf to PPU memory once nothing
// more will patch it (the next TXT block, or ENDMOD, is reached);
// PPU_F_WRITE accepts any address/length, not just a single
// whole-image blit, so this needs no reassembly on the CPU side at
// all. BSS (and any inter-psect alignment padding) is zero-filled
// directly on the PPU up front, in zero_fill_ppu(), before any TXT
// chunk is written -- see ppuc_load_code() below.
//
// This avoids a real constraint the previous two-buffer design had,
// found the hard way while testing ppus_send()/ppuc_recv() (see
// ppu_client.h): holding both buf and a same-order-of-magnitude
// content buffer at once meant combined peak size scaled with the
// *PPU* program being loaded, not the calling CPU program -- a CPU
// program that comfortably loaded a small PPU module could get
// ENOMEM loading a larger one, even with the PPU itself having plenty
// of free memory (confirmed directly: ppuc_alloc() for the same size,
// called on its own, succeeded -- the failure was the second
// malloc() on the CPU side). Halving the CPU-side buffer requirement
// removes that scaling entirely.

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ppu_client.h"
#include "ppuc_internal.h"

// Data block type codes and GSD/RLD entry types (RT-11 Volume and File
// Formats Manual, tables 2-1/2-2/2-5) -- same constants
// ld/emultempl/pdp11rt11rel.em's emitter uses.
#define REL_BLK_GSD 1
#define REL_BLK_ENDGSD 2
#define REL_BLK_TXT 3
#define REL_BLK_RLD 4
#define REL_BLK_ENDMOD 6

#define REL_GSD_PSECT 5

#define REL_RLD_INTERNAL 1
#define REL_RLD_INTERNAL_DISP 3
#define REL_RLD_LOCCTR_DEF 7
#define REL_RLD_PSECT 12
#define REL_RLD_PSECT_DISP 14
#define REL_RLD_PSECT_ADD 15
#define REL_RLD_PSECT_ADD_DISP 16

enum { PSECT_TEXT, PSECT_DATA, PSECT_BSS, PSECT_ABS, PSECT_COUNT };

// RADIX-50 packings of ".TEXT"/".DATA"/".BSS"/".ABS.", computed offline
// (same algorithm as the emitter's own rad50_pack6()) so this reader
// never needs its own RADIX-50 encoder -- these four names are the
// only ones it ever has to recognize (no per-symbol names -- see the
// file header comment on the entry point's fixed, convention-based
// location).
static const unsigned char kPsectNames[PSECT_COUNT][4] = {
    {0x25, 0xb2, 0x20, 0x99},  // .TEXT
    {0xa1, 0xaf, 0x28, 0x7d},  // .DATA
    {0x63, 0xaf, 0xc0, 0x76},  // .BSS
    {0x2a, 0xaf, 0x20, 0x7b},  // .ABS.
};

// One "formatted binary block" (manual's own term): a byte 1, a byte
// 0, a little-endian length covering that 4-byte prefix plus the
// payload (not the trailing checksum byte), the payload itself (whose
// first word is always a data-block-type code), and a checksum byte
// that is the negative of the sum of every preceding byte.
struct block {
  const unsigned char *body;  // payload, past its own leading type word
  unsigned int body_len;
  unsigned int type;
};

static unsigned int u16_le(const unsigned char *p) {
  return (unsigned int)(p[0] | (p[1] << 8));
}

// Advances *pos past one block, filling *out with its type and body.
// Returns 0 on any framing error (bad marker bytes, truncated block,
// bad checksum) -- the caller maps that to EINVAL.
static int next_block(const unsigned char *buf, unsigned int size,
                       unsigned int *pos, struct block *out) {
  unsigned int length, i, sum;
  unsigned char checksum;

  if (*pos + 4 > size || buf[*pos] != 1 || buf[*pos + 1] != 0) {
    return 0;
  }
  length = u16_le(buf + *pos + 2);
  if (length < 6 || *pos + length + 1 > size) {
    return 0;
  }
  sum = 0;
  for (i = 0; i < length; i++) {
    sum += buf[*pos + i];
  }
  checksum = buf[*pos + length];
  if (((sum + checksum) & 0377) != 0) {
    return 0;
  }
  out->type = u16_le(buf + *pos + 4);
  out->body = buf + *pos + 6;
  out->body_len = length - 6;
  *pos += length + 1;
  return 1;
}

static int find_psect(const unsigned char *name4) {
  int p;

  for (p = 0; p < PSECT_COUNT; p++) {
    if (memcmp(name4, kPsectNames[p], 4) == 0) {
      return p;
    }
  }
  return -1;
}

// Parses the GSD/ENDGSD block pair at *pos, filling psect_size[] from
// each PSECT-kind entry (global symbol definitions -- e.g. START, and
// now possibly others merged in from a second object file -- carry no
// information this reader needs; see the file header comment on the
// entry point's fixed location). Advances *pos past ENDGSD. Returns 1
// on success, 0 on any framing/checksum error.
static int parse_gsd(const unsigned char *buf, unsigned int size,
                      unsigned int *pos,
                      unsigned int psect_size[PSECT_COUNT]) {
  struct block blk;
  unsigned int i;
  int cur_psect;

  memset(psect_size, 0, PSECT_COUNT * sizeof(psect_size[0]));

  if (!next_block(buf, size, pos, &blk) || blk.type != REL_BLK_GSD) {
    return 0;
  }
  cur_psect = -1;
  for (i = 0; i + 8 <= blk.body_len; i += 8) {
    const unsigned char *name4 = blk.body + i;
    unsigned int typeword = u16_le(blk.body + i + 4);
    unsigned int value = u16_le(blk.body + i + 6);

    if ((typeword & 0377) == REL_GSD_PSECT) {
      cur_psect = find_psect(name4);
      if (cur_psect >= 0 && cur_psect != PSECT_ABS) {
        psect_size[cur_psect] = value;
      }
    }
  }
  return next_block(buf, size, pos, &blk) && blk.type == REL_BLK_ENDGSD;
}

// Applies every RLD entry in one RLD block, patching directly into
// buf -- specifically into *chunk_ptr, the current TXT chunk's own
// bytes at their original file position (set by the caller each time
// a new TXT block arrives; see parse_txt_rld()). A relocation patch
// is a pure overwrite (target_base + addend, computed from scratch),
// never a read-modify-write, so patching in place needs no PPU
// round-trip and no separate staging buffer -- the file's own REL
// convention (this toolchain's own emitter is the only producer this
// reader ever needs to handle) always emits a TXT chunk's relocations
// before any other TXT block for the same p-sect, so *chunk_ptr still
// points at the right bytes for every RLD block processed here.
//
// *cur_psect/*have_chunk carry the parser's running p-sect-tracking
// state across TXT/RLD blocks (a LOCCTR_DEF entry here updates
// *cur_psect and resets *have_chunk; a TXT block, handled by the
// caller, sets *have_chunk and chunk_ptr/chunk_len). Returns 1 on
// success, 0 on any malformed-file condition.
static int apply_rld_block(const struct block *blk,
                            const unsigned int psect_base[PSECT_COUNT],
                            unsigned char *chunk_ptr, unsigned int chunk_len,
                            int *cur_psect, int *have_chunk) {
  unsigned int j = 0;

  while (j + 2 <= blk->body_len) {
    unsigned int disp = blk->body[j];
    unsigned int rtype = blk->body[j + 1];
    unsigned int target_base, addend;
    unsigned short patched;
    int p;

    j += 2;
    if (rtype == REL_RLD_LOCCTR_DEF) {
      if (j + 6 > blk->body_len) {
        return 0;
      }
      p = find_psect(blk->body + j);
      if (p < 0 || p == PSECT_ABS) {
        return 0;
      }
      *cur_psect = p;
      *have_chunk = 0;
      j += 6;
      continue;
    }

    if (!*have_chunk || *cur_psect < 0 || *cur_psect == PSECT_ABS) {
      return 0;
    }
    if (disp + 2 > chunk_len || (disp & 1) != 0) {
      return 0;
    }

    switch (rtype) {
      case REL_RLD_INTERNAL:
        if (j + 2 > blk->body_len) {
          return 0;
        }
        addend = u16_le(blk->body + j);
        target_base = psect_base[*cur_psect];
        j += 2;
        break;
      case REL_RLD_PSECT:
      case REL_RLD_PSECT_ADD:
        if (j + 4 > blk->body_len) {
          return 0;
        }
        p = find_psect(blk->body + j);
        if (p < 0) {
          return 0;
        }
        target_base = psect_base[p];
        j += 4;
        addend = 0;
        if (rtype == REL_RLD_PSECT_ADD) {
          if (j + 2 > blk->body_len) {
            return 0;
          }
          addend = u16_le(blk->body + j);
          j += 2;
        }
        break;
      // PC-relative, same-module references (see the file header
      // comment): already correct as written, nothing to patch --
      // just consume the operand and move on to the next entry.
      case REL_RLD_INTERNAL_DISP:
        if (j + 2 > blk->body_len) {
          return 0;
        }
        j += 2;
        continue;
      case REL_RLD_PSECT_DISP:
      case REL_RLD_PSECT_ADD_DISP:
        if (j + 4 > blk->body_len) {
          return 0;
        }
        p = find_psect(blk->body + j);
        if (p < 0) {
          return 0;
        }
        j += 4;
        if (rtype == REL_RLD_PSECT_ADD_DISP) {
          if (j + 2 > blk->body_len) {
            return 0;
          }
          j += 2;
        }
        continue;
      default:
        // REL_RLD_GLOBAL/GLOBAL_ADD (a true external reference, by
        // name) and every remaining _DISP variant -- unsupported, see
        // the file header comment.
        return 0;
    }

    patched = (unsigned short)(target_base + addend);
    chunk_ptr[disp] = (unsigned char)patched;
    chunk_ptr[disp + 1] = (unsigned char)(patched >> 8);
  }

  return 1;
}

// Flushes one already-relocated TXT chunk from buf straight to PPU
// memory at chunk_ppu_addr, copying it through a small, fixed-size
// staging buffer first rather than handing chunk_ptr to
// ppuc_write_buf() directly -- chunk_ptr points into the middle of
// buf (at whatever byte offset the file's own block framing happened
// to land on), which PPU_F_WRITE's word-count transfer cannot rely on
// being even, unlike the previous design's content[] buffer (always a
// fresh malloc(), which this target's allocator always returns
// word-aligned). kStage is declared as unsigned short for the same
// reason struct rt11_file's own transfer buffer is (see the rt11
// newlib port's syscalls) -- it guarantees the even alignment a
// word-based transfer needs; only the memcpy destination itself is
// ever addressed byte-wise.
//
// Rounding the last piece's write length up to even when it's odd
// reads one extra (harmless) byte from buf -- always in bounds, since
// a TXT block's body has its own checksum byte right after it -- and
// writes one extra padding byte on the PPU side, which is always
// safe: psect_off's own even-alignment rounding guarantees at least
// one padding byte after any odd-sized p-sect, and zero_fill_ppu()
// (see ppuc_load_code()) has already covered it.
static int flush_chunk(unsigned short chunk_ppu_addr,
                        const unsigned char *chunk_ptr,
                        unsigned int chunk_len) {
  static unsigned short kStageWords[64];
  unsigned char *const kStage = (unsigned char *)kStageWords;
  unsigned int off = 0, piece, wlen;

  while (off < chunk_len) {
    piece = chunk_len - off;
    if (piece > sizeof(kStageWords)) {
      piece = sizeof(kStageWords);
    }
    memcpy(kStage, chunk_ptr + off, piece);
    wlen = piece;
    if (off + piece >= chunk_len && (wlen & 1) != 0) {
      kStage[wlen] = chunk_ptr[off + wlen];  // checksum byte -- see above
      wlen++;
    }
    if (!ppuc_write_buf((unsigned short)(chunk_ppu_addr + off), kStage,
                         wlen)) {
      return 0;
    }
    off += piece;
  }
  return 1;
}

// Reads TXT/RLD blocks starting at *pos (already positioned past
// GSD/ENDGSD by parse_gsd()) up through ENDMOD, patching every
// relocation directly into buf (apply_rld_block()) and writing each
// TXT chunk to PPU memory (flush_chunk()) once nothing more will
// patch it -- deferred until the next TXT block or ENDMOD, since an
// RLD block for the current chunk can still follow it. Returns 1 on
// success, 0 on any malformed-file condition or PPU write failure
// (errno is set to EIO for the latter; the caller doesn't need to set
// it again).
static int parse_txt_rld(unsigned char *buf, unsigned int size,
                          unsigned int *pos,
                          const unsigned int psect_size[PSECT_COUNT],
                          const unsigned int psect_off[PSECT_COUNT],
                          const unsigned int psect_base[PSECT_COUNT],
                          unsigned int content_size) {
  struct block blk;
  int cur_psect = -1;
  int have_chunk = 0;
  int pending = 0;
  unsigned char *chunk_ptr = NULL;
  unsigned int chunk_len = 0;
  unsigned short chunk_ppu_addr = 0;

  for (;;) {
    if (!next_block(buf, size, pos, &blk)) {
      errno = EINVAL;
      return 0;
    }
    if (blk.type == REL_BLK_ENDMOD) {
      if (pending && !flush_chunk(chunk_ppu_addr, chunk_ptr, chunk_len)) {
        errno = EIO;
        return 0;
      }
      return 1;
    }
    if (blk.type == REL_BLK_TXT) {
      unsigned int cs = u16_le(blk.body);
      unsigned int n2 = blk.body_len - 2;

      if (pending && !flush_chunk(chunk_ppu_addr, chunk_ptr, chunk_len)) {
        errno = EIO;
        return 0;
      }
      pending = 0;

      if (cur_psect < 0 || cur_psect == PSECT_ABS ||
          cs + n2 > psect_size[cur_psect] ||
          psect_off[cur_psect] + cs + n2 > content_size) {
        errno = EINVAL;
        return 0;
      }
      chunk_ptr = (unsigned char *)blk.body + 2;
      chunk_len = n2;
      chunk_ppu_addr = (unsigned short)(psect_base[cur_psect] + cs);
      have_chunk = 1;
      pending = 1;
    } else if (blk.type == REL_BLK_RLD) {
      if (!apply_rld_block(&blk, psect_base, chunk_ptr, chunk_len, &cur_psect,
                            &have_chunk)) {
        errno = EINVAL;
        return 0;
      }
    } else {
      errno = EINVAL;
      return 0;
    }
  }
}

// Zero-fills size bytes of PPU memory starting at ppu_addr, in fixed
// small chunks -- covers BSS and any inter-psect alignment padding,
// which (unlike .text/.data) has no bytes of its own in the file to
// copy from. Called once, up front, before any TXT chunk is written,
// so every byte a TXT chunk doesn't touch is left zero rather than
// whatever ppuc_alloc()'s block happened to already hold.
static int zero_fill_ppu(unsigned short ppu_addr, unsigned int size) {
  // unsigned short, not unsigned char[] -- see flush_chunk()'s own
  // kStageWords for why: PPU_F_WRITE's word-count transfer needs an
  // even source address, which only a genuinely word-typed buffer is
  // guaranteed to have.
  static const unsigned short kZeroWords[64] = {0};
  const unsigned char *const kZero = (const unsigned char *)kZeroWords;
  unsigned int off = 0, chunk;

  while (off < size) {
    chunk = size - off;
    if (chunk > sizeof(kZeroWords)) {
      chunk = sizeof(kZeroWords);
    }
    if (!ppuc_write_buf((unsigned short)(ppu_addr + off), kZero, chunk)) {
      return 0;
    }
    off += chunk;
  }
  return 1;
}

long ppuc_load_code(const char *name) {
  int fd;
  struct stat st;
  unsigned char *buf;
  unsigned int size, pos;
  int n, file_pos;
  unsigned int psect_size[PSECT_COUNT];
  unsigned int psect_off[PSECT_COUNT];  // byte offset within the PPU image
  unsigned int psect_base[PSECT_COUNT];
  unsigned int content_size;
  long alloc_result;
  unsigned short ppu_addr;

  fd = open(name, O_RDONLY, 0);
  if (fd < 0) {
    return -1;
  }
  if (fstat(fd, &st) < 0) {
    close(fd);
    return -1;
  }
  size = (unsigned int)st.st_size;
  buf = malloc(size > 0 ? size : 1);
  if (buf == NULL) {
    close(fd);
    errno = ENOMEM;
    return -1;
  }
  file_pos = 0;
  while ((unsigned int)file_pos < size) {
    n = read(fd, buf + file_pos, size - (unsigned int)file_pos);
    if (n <= 0) {
      break;
    }
    file_pos += n;
  }
  close(fd);
  if ((unsigned int)file_pos != size) {
    free(buf);
    errno = EIO;
    return -1;
  }

  pos = 0;
  if (!parse_gsd(buf, size, &pos, psect_size)) {
    free(buf);
    errno = EINVAL;
    return -1;
  }

  // ---- Lay out .text/.data/.bss consecutively in one PPU block ----
  psect_off[PSECT_TEXT] = 0;
  psect_off[PSECT_DATA] = (psect_size[PSECT_TEXT] + 1) & ~1u;
  psect_off[PSECT_BSS] =
      psect_off[PSECT_DATA] + ((psect_size[PSECT_DATA] + 1) & ~1u);
  content_size = psect_off[PSECT_BSS] + ((psect_size[PSECT_BSS] + 1) & ~1u);
  if (content_size == 0) {
    free(buf);
    errno = EINVAL;
    return -1;
  }

  alloc_result = ppuc_alloc(content_size);
  if (alloc_result < 0) {
    free(buf);
    return -1;  // errno already set by ppuc_alloc
  }
  ppu_addr = (unsigned short)alloc_result;
  psect_base[PSECT_TEXT] = ppu_addr + psect_off[PSECT_TEXT];
  psect_base[PSECT_DATA] = ppu_addr + psect_off[PSECT_DATA];
  psect_base[PSECT_BSS] = ppu_addr + psect_off[PSECT_BSS];
  psect_base[PSECT_ABS] = 0;

  if (!zero_fill_ppu(ppu_addr, content_size)) {
    ppuc_free(ppu_addr);
    free(buf);
    errno = EIO;
    return -1;
  }

  // parse_txt_rld() patches relocations directly into buf and writes
  // each already-relocated TXT chunk straight to PPU memory as it
  // goes (see its own header comment) -- sets errno itself (EINVAL or
  // EIO) on any failure.
  if (!parse_txt_rld(buf, size, &pos, psect_size, psect_off, psect_base,
                      content_size)) {
    ppuc_free(ppu_addr);
    free(buf);
    return -1;
  }

  free(buf);
  return ppu_addr;
}
