#include <fcntl.h>
#include <unistd.h>

int main(void) {
  int fd;
  char buf[64];
  int n;
  static const char msg[] = "Hello, RT-11 file!\r\n";

  fd = open("GREET.TXT", O_WRONLY | O_CREAT | O_TRUNC, 0666);
  write(fd, msg, sizeof(msg) - 1);
  close(fd);

  fd = open("GREET.TXT", O_RDONLY, 0);
  n = read(fd, buf, sizeof(buf));
  close(fd);

  write(STDOUT_FILENO, buf, (unsigned int) n);

  return 0;
}
