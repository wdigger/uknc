// ppu_client.h -- API for the part of libppu that runs on the CPU
//
// The CPU side: loads a program into PPU memory and starts it, talking
// to the PPU over the CPU<->PPU channel port (176674/176676), following
// the same request protocol as Oleg Safiullin's PRUN utility
// (pdp-11.org.ru/~form/files/pdp-11/uknc/prun/).

#ifndef PPU_CLIENT_H
#define PPU_CLIENT_H

// One entry in the relocation table passed to ppuc_load_reloc(): offset
// is the byte offset (must be even) within the caller's buffer of a
// 16-bit word that needs the buffer's eventual PPU base address added
// to it (subtract == 0) or subtracted from it (subtract != 0) once
// that address is known -- mirrors PRUN's own RELOC table (offset + a
// sign bit, packed differently there, but the same two pieces of
// information). The word's own current value is taken as already
// relocated for a base of 0.
struct ppu_reloc {
  unsigned short offset;
  unsigned char subtract;
};

// Allocates size bytes of PPU memory. Returns the PPU address of the
// new block (always a small, non-negative value: PPU addresses fit in
// 16 bits) on success, or -1 on error with errno set:
//   EINVAL  size is zero or odd
//   ENOMEM  the PPU has no size contiguous bytes free
long ppuc_alloc(unsigned int size);

// Releases a PPU memory block previously returned by ppuc_alloc() (or by
// ppuc_load()/ppuc_load_reloc()). Returns 0 on success, or -1 on error
// with errno set:
//   EIO  the PPU did not accept the deallocation request
int ppuc_free(unsigned short ppu_addr);

// Starts code running on the PPU at ppu_addr -- the address any
// ppuc_load*() call returned. A well-behaved PPU program frees its own
// memory block and returns to
// the PPU's resident monitor when done (jumping to address 0176300
// with R1 set to the block's base address -- see PRUN's own
// convention; ppu_server.h, not yet implemented, will cover the PPU
// side of this).
//
// Returns 0 on success, or -1 on error with errno set:
//   EIO  the PPU did not accept the run request
int ppuc_run(unsigned short ppu_addr);

// Allocates size bytes of PPU memory (see ppuc_alloc()) and copies buf
// into it unmodified -- for code that's already position-independent
// and needs no relocation; see ppuc_load_reloc() otherwise.
//
// Returns the PPU address the buffer was loaded at on success, or -1 on
// error with errno set:
//   EINVAL  size is zero or odd
//   ENOMEM  the PPU has no size contiguous bytes free
//   EIO     the PPU did not accept the load (request/protocol error)
// On any of these errors, no PPU memory is left allocated.
//
// Does not start the loaded code -- see ppuc_run() for that.
long ppuc_load(const void *buf, unsigned int size);

// Like ppuc_load(), but first patches *buf according to the
// reloc_count entries at reloc (each word named by a ppu_reloc gets
// the newly-allocated PPU base address added to or subtracted from its
// current value) before copying it into PPU memory.
//
// reloc may be NULL -- e.g. position-independent code needs no
// patching at all, that's not an error -- in which case reloc_count is
// ignored (there's nothing to index into it with), whatever its value.
//
// Same return value and errno set as ppuc_load(), plus:
//   EINVAL  also set if (with reloc non-NULL) a reloc entry's offset
//           is odd or falls outside the buffer
long ppuc_load_reloc(const void *buf, unsigned int size,
                     const struct ppu_reloc *reloc,
                     unsigned int reloc_count);

// Reads name's contents into a temporary CPU-side buffer and loads
// them into PPU memory unmodified (see ppuc_load()) -- no relocation;
// use ppuc_load() directly (after reading the file yourself) if the
// buffer also needs patching.
//
// size caps how many bytes of the file actually matter: pass 0 to use
// the whole file (its size as reported by fstat()) -- RT-11 files are
// block-aligned (512 bytes), so a program image's real payload is
// often smaller than that, with trailing padding in the last block;
// pass the real byte count to load just that much and ignore the rest.
//
// Returns the PPU address the file was loaded at on success, or -1 on
// error with errno set to whatever open()/fstat()/read() set it to, or:
//   EINVAL  size is larger than the file itself
//   ENOMEM  not enough CPU memory for a temporary read buffer, or (see
//           ppuc_load()) not enough PPU memory
//   EIO     a short read, or (see ppuc_load()) the PPU did not accept
//           the load
long ppuc_load_file(const char *name, unsigned int size);

// Reads name -- an RT-11 native relocatable object module ("REL":
// GSD/TXT/RLD/ENDMOD blocks), produced directly by this toolchain's
// own linker with no extra tooling:
//
//   pdp11-uknc-rt11-as foo.s -o foo.o
//   pdp11-uknc-rt11-ld -r -m pdp11rt11rel foo.o -o foo.ppu
//
// -- and loads it into PPU memory, relocated against the
// newly-allocated address. This is a small single-module linker, not
// a generic reader of every RT-11 REL file: it only understands the
// fixed .text/.data/.bss/.ABS. p-sect model and the relocation subset
// this toolchain's own pdp11rt11rel emitter ever writes for a
// self-contained module with no unresolved external references. The
// object format itself has no separate "transfer address" field,
// unlike a fully linked image -- but this project's own convention
// fixes the entry point at .text offset 0 regardless (see
// ppu_server.h/ppus_start.c), so the returned load address doubles as
// the address to pass to ppuc_run(); no separate lookup is needed.
//
// Returns the PPU address the module was loaded (and should be run)
// at on success, or -1 on error with errno set to whatever
// open()/fstat()/read() set it to, or:
//   EINVAL  the file isn't a well-formed REL module (bad block
//           framing or checksum, a p-sect name this reader doesn't
//           recognize, an unsupported relocation entry, an
//           out-of-range relocation)
//   ENOMEM  not enough CPU memory for a temporary read buffer, or (see
//           ppuc_load()) not enough PPU memory
//   EIO     a short read, or (see ppuc_load()) the PPU did not accept
//           the load
long ppuc_load_code(const char *name);

#endif  // PPU_CLIENT_H
