/* fileio.c -- sequential file I/O demo for pdp11-uknc-rt11
 *
 * Writes a small file, closes it, reopens it for reading, and verifies
 * every byte comes back unchanged, using varying (non-block-aligned)
 * read sizes to exercise the buffering in newlib/libc/sys/rt11/syscalls.c.
 * See ../lseek/lseek.c for the matching lseek() exercises and ../rdwr/
 * rdwr.c for O_RDWR/overwrite-in-place -- kept as separate programs
 * rather than folded in here: combining them pushed a linked .sav past
 * available RAM (RT-11's fixed stack-top minus a program's own .bss
 * leaves only so much headroom) and it hung mid-run instead of failing
 * cleanly.
 *
 * Uses the plain POSIX names (open/read/write/close/fstat), now that
 * newlib/configure.host's syscall_dir=syscalls for pdp11 builds
 * libc/syscalls' thin connectors (open() calling _open(), etc.) on top
 * of the underscore-prefixed syscall layer -- essentially free (no
 * buffering, no FILE* struct) compared to calling _open/_read/... by
 * hand.
 *
 * Console messages use write(1, ...) directly, NOT puts()/stdio: this
 * project found that mixing buffered stdio with a multi-byte read() call
 * that crosses a 512-byte block boundary can still corrupt data or hang
 * (a real, still-unresolved backend/runtime issue on this rare pdp11
 * target -- a related, narrower case of it was root-caused to a per-byte
 * 32-bit division in _read()/_write() and fixed, but this exact shape --
 * this file's own 512-byte chunk in its read-size cycle -- still
 * triggers something, and pinning it down further is its own separate
 * task).  write(1, ...) sidesteps it entirely, and is what puts() calls
 * internally anyway.  No printf()/sprintf() either way: GCC's pdp11
 * backend can't make _printf_float weak, so any reference at all drags
 * in the float formatting machinery and overflows UKNC's 64K address
 * space.
 */

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define FILE_SIZE 1234 /* 2 full 512-byte blocks + a partial tail */

static void msg(const char *s) {
  int n = 0;

  while (s[n]) n++;
  write(1, s, (size_t)n);
  write(1, "\r\n", 2);
}

int main(void) {
  static char wbuf[FILE_SIZE];
  static char rbuf[FILE_SIZE];
  int fd, i, ok = 1;
  int r;
  struct stat st;

  for (i = 0; i < FILE_SIZE; i++) wbuf[i] = (char)(i & 0xff);

  msg("start");

  fd = open("TEST.DAT", O_WRONLY | O_CREAT, 0666);
  if (fd < 0) {
    msg("open(W) failed");
    return 1;
  }
  r = write(fd, wbuf, sizeof(wbuf));
  if (r != (int)sizeof(wbuf)) {
    msg("write short");
    ok = 0;
  }
  close(fd);

  fd = open("TEST.DAT", O_RDONLY, 0);
  if (fd < 0) {
    msg("open(R) failed");
    return 1;
  }

  /* .LOOKUP only knows a file's length in whole 512-byte blocks, so
     st_size here rounds FILE_SIZE up to the next block boundary --
     read past FILE_SIZE legitimately returns that block's padding,
     not EOF, until the last allocated block is drained.  */
  fstat(fd, &st);

  {
    int pos = 0;
    static const int chunks[5] = {7, 100, 512, 1, 37};
    int ci = 0;

    while (pos < FILE_SIZE) {
      int want = chunks[ci % 5];

      if (want > FILE_SIZE - pos) want = FILE_SIZE - pos;
      r = read(fd, rbuf + pos, want);
      if (r <= 0) break;
      pos += r;
      ci++;
    }
    if (pos != FILE_SIZE) {
      msg("read short");
      ok = 0;
    }
  }

  for (i = 0; i < FILE_SIZE; i++) {
    if (rbuf[i] != wbuf[i]) {
      msg("mismatch");
      ok = 0;
      break;
    }
  }

  /* Drain the current block's trailing padding before expecting true
     EOF -- see the st_size comment above.  */
  while ((off_t)FILE_SIZE < st.st_size) {
    r = read(fd, rbuf, sizeof(rbuf));
    if (r <= 0) break;
    st.st_size -= r;
  }

  r = read(fd, rbuf, sizeof(rbuf));
  if (r != 0) {
    msg("expected EOF");
    ok = 0;
  }

  close(fd);

  msg(ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
