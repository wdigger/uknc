// ppuc_load_file.c -- read a file and load its contents into PPU memory

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ppu_client.h"

long ppuc_load_file(const char *name, unsigned int size) {
  int fd;
  struct stat st;
  void *buf;
  int pos;
  int n;
  long result;

  fd = open(name, O_RDONLY, 0);
  if (fd < 0) {
    return -1;
  }

  if (fstat(fd, &st) < 0) {
    close(fd);
    return -1;
  }

  // size == 0 means "the whole file" -- RT-11 files are block-aligned
  // (512 bytes), so st.st_size may already include trailing padding
  // past the real payload; a caller that knows the real byte count
  // passes it here instead to load just that much.
  if (size == 0) {
    size = (unsigned int)st.st_size;
  } else if (size > (unsigned int)st.st_size) {
    close(fd);
    errno = EINVAL;
    return -1;
  }

  buf = malloc(size);
  if (buf == NULL) {
    close(fd);
    errno = ENOMEM;
    return -1;
  }

  pos = 0;
  while ((unsigned int)pos < size) {
    n = read(fd, (char *)buf + pos, size - pos);
    if (n <= 0) {
      break;
    }
    pos += n;
  }
  close(fd);
  if ((unsigned int)pos != size) {
    free(buf);
    errno = EIO;
    return -1;
  }

  result = ppuc_load(buf, size);
  free(buf);
  return result;
}
