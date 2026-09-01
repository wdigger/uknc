/* rdwr.c -- random-access write / O_RDWR / overwrite-in-place demo for
 * pdp11-uknc-rt11
 *
 * Exercises the parts of newlib/libc/sys/rt11/syscalls.c that only work
 * because every file now goes through one arbitrary-position buffer
 * (rt11_ensure_block), not a single fixed read-or-write direction:
 *
 *   - O_RDWR: open once, write, lseek backward, overwrite, lseek to the
 *     start, read the whole file back -- all on the same fd, no close
 *     in between.
 *   - Overwrite-in-place: close, reopen O_WRONLY (no O_TRUNC) on the
 *     now-existing file, overwrite a different byte range via lseek+
 *     write, close, reopen O_RDONLY, and confirm the WHOLE file -- both
 *     edited ranges plus every untouched byte -- reads back correctly.
 *     This is what proves the file wasn't silently recreated/truncated.
 *   - O_WRONLY without O_CREAT on a name that doesn't exist yet -> a
 *     real ENOENT, not a silent create (the .LOOKUP-first bug fixed
 *     alongside this feature).
 *
 * Uses write(1, ...) for console messages rather than puts()/stdio --
 * not required for correctness (a real miscompilation that this exact
 * kind of test surfaced, a per-byte 32-bit division/modulo recomputing
 * blk/off_in_blk from f->pos on every byte, has since been fixed in
 * _read()/_write() by deriving them once per call and advancing by
 * plain increment instead -- see the comment there), just consistent
 * with this project's existing style for small test helpers.
 */

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define FILE_SIZE 700 /* 1 full 512-byte block + a partial tail */

static void msg(const char *s) {
  int n = 0;

  while (s[n]) n++;
  write(1, s, (size_t)n);
  write(1, "\r\n", 2);
}

static void msgnum(const char *pre, int v) {
  char buf[8];
  int i = 0;
  unsigned int u = (unsigned int)v;

  msg(pre);
  if (u == 0) buf[i++] = '0';
  while (u != 0) {
    buf[i++] = (char)('0' + (u % 10));
    u /= 10;
  }
  while (i > 0) write(1, &buf[--i], 1);
  write(1, "\r\n", 2);
}

int main(void) {
  static char wbuf[FILE_SIZE];
  static char rbuf[FILE_SIZE];
  int fd, i, ok = 1;
  int r;

  for (i = 0; i < FILE_SIZE; i++) wbuf[i] = (char)(i & 0xff);

  /* O_RDWR: create, write, seek back, overwrite, seek to start, read
     back the whole file -- all on one fd, no close in between. */
  fd = open("RDW.DAT", O_RDWR | O_CREAT, 0666);
  if (fd < 0) {
    msg("open1 failed");
    return 1;
  }
  r = write(fd, wbuf, FILE_SIZE);
  if (r != FILE_SIZE) {
    msg("write1 short");
    ok = 0;
  }

  if (lseek(fd, 100, SEEK_SET) != 100) {
    msg("lseek1 failed");
    ok = 0;
  }
  for (i = 0; i < 50; i++) wbuf[100 + i] = (char)(0xA0 + i);
  r = write(fd, wbuf + 100, 50);
  if (r != 50) {
    msg("write2 short");
    ok = 0;
  }

  if (lseek(fd, 0, SEEK_SET) != 0) {
    msg("lseek2 failed");
    ok = 0;
  }
  r = read(fd, rbuf, FILE_SIZE);
  if (r != FILE_SIZE) {
    msgnum("read1 short r=", r);
    ok = 0;
  }
  for (i = 0; i < FILE_SIZE; i++) {
    if (rbuf[i] != wbuf[i]) {
      msgnum("mismatch same-fd at ", i);
      ok = 0;
      break;
    }
  }
  close(fd);

  /* Overwrite-in-place: O_WRONLY without O_TRUNC on the existing file
     must preserve everything outside the range actually written. */
  fd = open("RDW.DAT", O_WRONLY, 0);
  if (fd < 0) {
    msg("open2 failed");
    return 1;
  }
  if (lseek(fd, 200, SEEK_SET) != 200) {
    msg("lseek3 failed");
    ok = 0;
  }
  for (i = 0; i < 50; i++) wbuf[200 + i] = (char)(0xC0 + i);
  r = write(fd, wbuf + 200, 50);
  if (r != 50) {
    msg("write3 short");
    ok = 0;
  }
  close(fd);

  fd = open("RDW.DAT", O_RDONLY, 0);
  if (fd < 0) {
    msg("open3 failed");
    return 1;
  }
  r = read(fd, rbuf, FILE_SIZE);
  if (r != FILE_SIZE) {
    msgnum("read2 short r=", r);
    ok = 0;
  }
  for (i = 0; i < FILE_SIZE; i++) {
    if (rbuf[i] != wbuf[i]) {
      msgnum("mismatch after reopen at ", i);
      ok = 0;
      break;
    }
  }
  close(fd);

  /* O_WRONLY without O_CREAT on a nonexistent name -> ENOENT, not a
     silent create. */
  errno = 0;
  fd = open("NOPE.DAT", O_WRONLY, 0);
  if (fd >= 0) {
    msg("nonexistent open unexpectedly succeeded");
    ok = 0;
    close(fd);
  } else if (errno != ENOENT) {
    msgnum("wrong errno for nonexistent, errno=", errno);
    ok = 0;
  }

  msg(ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
