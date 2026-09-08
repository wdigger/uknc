// ppuc_rel.c -- the RT-11 native relocatable object module ("REL":
// GSD/TXT/RLD/ENDMOD blocks) reader shared by every PPU-program loader:
// ppuc_load_code()/ppuc_load_code_at() read one from a file, and
// ppuc_load_code_buf() takes one the caller already has in memory. See
// ppuc_load_code.c for the format's own background, for the relocation
// subset this understands, and for why the entry point is always .text
// offset 0.
//
// Nothing here opens a file, allocates, or touches errno, so a program
// that only ever calls ppuc_load_code_buf() -- a module linked into its
// own image, say -- links none of the C library on this account at all.
// The file loaders add exactly that much on top.
//
// The module is read strictly forwards, and each TXT chunk is copied
// into chunk_words below before its relocations are patched in, so the
// caller's own bytes are never written to and may well be const.

#include <string.h>

#include "ppu_client.h"
#include "ppuc_internal.h"

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

// Applies every RLD entry in one RLD block, patching into *chunk_ptr:
// the copy of the current TXT chunk the caller made when that block
// arrived (see chunk_words in parse_txt_rld()). A relocation patch is
// a pure overwrite (target_base + addend, computed from scratch),
// never a read-modify-write, so it needs no PPU round-trip -- and the
// REL convention (this toolchain's own emitter is the only producer
// this reader ever needs to handle) always emits a TXT chunk's
// relocations before any other TXT block for the same p-sect, so that
// copy is still the right one for every RLD block processed here.
//
// *cur_psect/*have_chunk carry the parser's running p-sect-tracking
// state across TXT/RLD blocks (a LOCCTR_DEF entry here updates
// *cur_psect and resets *have_chunk; a TXT block, handled by the
// caller, sets *have_chunk and the chunk copy). Returns 1 on success,
// 0 on any malformed-module condition.
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

// The TXT chunk being worked on, copied out of the module: relocations
// are patched into this copy, never into the caller's own bytes, so a
// module may just as well be a const array linked into the program
// (ppuc_load_code_buf()) as a freshly read file. unsigned short for the
// even alignment PPU_F_WRITE's word transfer needs -- only the memcpy
// destination is ever addressed byte-wise. REL_TXT_CHUNK is what
// pdp11rt11rel.em's own emitter chunks a p-sect into; a module with a
// bigger chunk is rejected rather than quietly truncated.
#define REL_TXT_CHUNK 128
static unsigned short chunk_words[REL_TXT_CHUNK / 2];

// Writes the current chunk, already relocated, to PPU memory. An odd
// length is padded to a whole word: psect_off's own even-alignment
// rounding leaves at least one spare byte after any odd-sized p-sect,
// and zero_fill_ppu() has already covered it.
static int flush_chunk(unsigned short chunk_ppu_addr, unsigned int chunk_len) {
  unsigned char *const chunk = (unsigned char *)chunk_words;

  if ((chunk_len & 1) != 0) {
    chunk[chunk_len++] = 0;
  }
  return ppuc_write_buf(chunk_ppu_addr, chunk, chunk_len);
}

// Reads TXT/RLD blocks starting at *pos (already positioned past
// GSD/ENDGSD by parse_gsd()) up through ENDMOD, patching every
// relocation into the chunk copy (apply_rld_block()) and writing that
// chunk to PPU memory (flush_chunk()) once nothing more will patch it
// -- deferred until the next TXT block or ENDMOD, since an RLD block
// for the current chunk can still follow it. Returns 1 on success, 0 on
// any malformed-module condition or on a failed PPU write; what that
// means to the caller is the caller's own business.
static int parse_txt_rld(const unsigned char *buf, unsigned int size,
                          unsigned int *pos,
                          const unsigned int psect_size[PSECT_COUNT],
                          const unsigned int psect_off[PSECT_COUNT],
                          const unsigned int psect_base[PSECT_COUNT],
                          unsigned int content_size) {
  struct block blk;
  int cur_psect = -1;
  int have_chunk = 0;
  int pending = 0;
  unsigned int chunk_len = 0;
  unsigned short chunk_ppu_addr = 0;

  for (;;) {
    if (!next_block(buf, size, pos, &blk)) {
      return 0;
    }
    if (blk.type == REL_BLK_ENDMOD) {
      if (pending && !flush_chunk(chunk_ppu_addr, chunk_len)) {
        return 0;
      }
      return 1;
    }
    if (blk.type == REL_BLK_TXT) {
      unsigned int cs = u16_le(blk.body);
      unsigned int n2 = blk.body_len - 2;

      if (pending && !flush_chunk(chunk_ppu_addr, chunk_len)) {
        return 0;
      }
      pending = 0;

      if (cur_psect < 0 || cur_psect == PSECT_ABS || n2 > sizeof(chunk_words) ||
          cs + n2 > psect_size[cur_psect] ||
          psect_off[cur_psect] + cs + n2 > content_size) {
        return 0;
      }
      memcpy(chunk_words, blk.body + 2, n2);
      chunk_len = n2;
      chunk_ppu_addr = (unsigned short)(psect_base[cur_psect] + cs);
      have_chunk = 1;
      pending = 1;
    } else if (blk.type == REL_BLK_RLD) {
      if (!apply_rld_block(&blk, psect_base, (unsigned char *)chunk_words,
                            chunk_len, &cur_psect, &have_chunk)) {
        return 0;
      }
    } else {
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
  // unsigned short, not unsigned char[] -- see chunk_words above for
  // why: PPU_F_WRITE's word-count transfer needs an even source
  // address, which only a genuinely word-typed buffer is guaranteed to
  // have.
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

// Lays the three p-sects out consecutively from a load address of 0:
// psect_off[] gets each one's byte offset within the image, and the
// return value is the whole image's size (0 for an empty or malformed
// module).
static unsigned int layout(const unsigned int psect_size[PSECT_COUNT],
                            unsigned int psect_off[PSECT_COUNT]) {
  psect_off[PSECT_TEXT] = 0;
  psect_off[PSECT_DATA] = (psect_size[PSECT_TEXT] + 1) & ~1u;
  psect_off[PSECT_BSS] =
      psect_off[PSECT_DATA] + ((psect_size[PSECT_DATA] + 1) & ~1u);
  return psect_off[PSECT_BSS] + ((psect_size[PSECT_BSS] + 1) & ~1u);
}

unsigned int ppuc_rel_content_size(const void *module, unsigned int size) {
  unsigned int psect_size[PSECT_COUNT], psect_off[PSECT_COUNT], pos = 0;

  if (!parse_gsd((const unsigned char *)module, size, &pos, psect_size)) {
    return 0;
  }
  return layout(psect_size, psect_off);
}

int ppuc_rel_load(const void *module, unsigned int size, unsigned short at) {
  const unsigned char *buf = (const unsigned char *)module;
  unsigned int psect_size[PSECT_COUNT], psect_off[PSECT_COUNT];
  unsigned int psect_base[PSECT_COUNT], content_size, pos = 0;

  if (!parse_gsd(buf, size, &pos, psect_size)) {
    return 0;
  }
  content_size = layout(psect_size, psect_off);
  if (content_size == 0) {
    return 0;
  }
  psect_base[PSECT_TEXT] = at + psect_off[PSECT_TEXT];
  psect_base[PSECT_DATA] = at + psect_off[PSECT_DATA];
  psect_base[PSECT_BSS] = at + psect_off[PSECT_BSS];
  psect_base[PSECT_ABS] = 0;

  // BSS and any inter-p-sect padding have no bytes of their own in the
  // module: zero them first, then let every TXT chunk land on top.
  if (!zero_fill_ppu(at, content_size)) {
    return 0;
  }
  return parse_txt_rld(buf, size, &pos, psect_size, psect_off, psect_base,
                        content_size);
}
