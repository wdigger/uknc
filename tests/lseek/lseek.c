/* lseek.c -- lseek() demo/exerciser for pdp11-uknc-rt11
 *
 * Writes a small file that spans two 512-byte blocks, then exercises
 * lseek() on the reopened read-mode fd: SEEK_SET to a mid-file offset,
 * SEEK_CUR crossing the block boundary (forcing rt11_ensure_block() in
 * newlib/libc/sys/rt11/syscalls.c to actually refetch rather than reuse
 * the buffered block), SEEK_END with a negative offset, an out-of-range
 * target (EINVAL), lseek on a console fd (ESPIPE, not ENOSYS), and lseek
 * on a write-mode fd (now supported -- random-access write via
 * rt11_ensure_block's read-modify-write; see ../rdwr/rdwr.c for the
 * fuller O_RDWR/overwrite-in-place exercises).
 *
 * Kept as its own small program rather than folded into ../fileio/
 * fileio.c: combining both there pushed the linked .sav past available
 * RAM and it hung mid-run instead of failing cleanly (RT-11's fixed
 * stack-top minus a program's own .bss leaves only so much headroom on
 * this 64K target), so FILE_SIZE here is deliberately small -- just
 * enough to cross one block boundary once.
 */

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#define FILE_SIZE 700 /* 1 full 512-byte block + a partial tail */

static void putline_num(const char *prefix, int num) {
  char line[32];
  char digits[8];
  int i = 0, j = 0, k = 0;
  int neg = num < 0;
  unsigned int unum = neg ? (unsigned int)-num : (unsigned int)num;

  while (prefix[j]) line[i++] = prefix[j++];
  if (unum == 0) digits[k++] = '0';
  while (unum != 0) {
    digits[k++] = (char)('0' + (unum % 10));
    unum /= 10;
  }
  if (neg) line[i++] = '-';
  while (k > 0) line[i++] = digits[--k];
  line[i] = '\0';
  puts(line);
}

int main(void) {
  static char wbuf[FILE_SIZE];
  int fd, i, ok = 1;
  int fail = 0;
  struct stat st;
  off_t off;
  char c;

  for (i = 0; i < FILE_SIZE; i++) wbuf[i] = (char)(i & 0xff);

  puts("start");

  fd = open("LSK.DAT", O_WRONLY | O_CREAT, 0666);
  if (fd < 0) {
    puts("open(W) failed");
    return 1;
  }
  if (write(fd, wbuf, sizeof(wbuf)) != (int)sizeof(wbuf)) {
    puts("write short");
    ok = 0;
  }
  close(fd);

  fd = open("LSK.DAT", O_RDONLY, 0);
  if (fd < 0) {
    puts("open(R) failed");
    return 1;
  }

  /* .LOOKUP only knows a file's length in whole 512-byte blocks, so
     st_size rounds FILE_SIZE up to the next block boundary. */
  fstat(fd, &st);

  /* SEEK_SET to a mid-file offset (still block 0, nothing buffered
     yet -- forces the very first rt11_read_block() fetch). */
  off = lseek(fd, 300, SEEK_SET);
  if (off != 300) fail |= 1;
  if (read(fd, &c, 1) != 1 || c != wbuf[300]) fail |= 2;

  /* SEEK_CUR crossing the 512-byte block boundary: land at 510 (still
     block 0), then jump +20 via SEEK_CUR to 530 (block 1) -- forces a
     real refetch rather than reusing the already-buffered block. */
  off = lseek(fd, 510, SEEK_SET);
  if (off != 510) fail |= 4;
  off = lseek(fd, 20, SEEK_CUR);
  if (off != 530) fail |= 8;
  if (read(fd, &c, 1) != 1 || c != wbuf[530]) fail |= 16;

  /* SEEK_END with a negative offset: fstat's block-rounded st_size
     means this may land in the last block's padding region, not
     necessarily inside wbuf -- just confirm the seek and read
     succeed, no byte comparison. */
  off = lseek(fd, -50, SEEK_END);
  if (off != st.st_size - 50) fail |= 32;
  if (read(fd, &c, 1) != 1) fail |= 64;

  /* Out-of-range target (past the block-rounded end): EINVAL. */
  errno = 0;
  if (lseek(fd, st.st_size + 1, SEEK_SET) != (off_t)-1 || errno != EINVAL)
    fail |= 128;

  /* Console fd: ESPIPE, not ENOSYS. */
  errno = 0;
  if (lseek(1, 0, SEEK_CUR) != (off_t)-1 || errno != ESPIPE) fail |= 256;

  close(fd);

  if (fail) {
    putline_num("lseek fail mask ", fail);
    ok = 0;
  }

  /* Write-mode fd: lseek now works -- write a byte, seek back over it,
     overwrite with a different value, seek to the start, and read back
     to confirm the overwrite landed correctly. */
  {
    int wfd = open("LSK2.DAT", O_WRONLY | O_CREAT, 0666);
    char wc = 0x11, rc;

    if (wfd < 0) {
      puts("open(W) LSK2 failed");
      ok = 0;
    } else {
      if (write(wfd, &wc, 1) != 1) {
        puts("LSK2 write1 failed");
        ok = 0;
      }
      if (lseek(wfd, 0, SEEK_SET) != 0) {
        puts("lseek write-mode failed");
        ok = 0;
      }
      wc = 0x22;
      if (write(wfd, &wc, 1) != 1) {
        puts("LSK2 write2 failed");
        ok = 0;
      }
      close(wfd);

      wfd = open("LSK2.DAT", O_RDONLY, 0);
      if (wfd < 0 || read(wfd, &rc, 1) != 1 || rc != (char)0x22) {
        puts("LSK2 readback mismatch");
        ok = 0;
      }
      if (wfd >= 0) close(wfd);
    }
  }

  puts(ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
