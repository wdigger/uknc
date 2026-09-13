/* uknc_exit.c -- report a program's exit status where the test harness
 * can see it.
 *
 * RT-11 has no notion of an exit status: a program ends with .EXIT and
 * the monitor prints its prompt, telling nothing about how it went.  The
 * DejaGnu board for this target therefore reads the status off the
 * screen, and this is what puts it there.  It replaces newlib's own
 * _exit, which every path out of a program goes through -- main
 * returning, exit(), abort() -- by being named on the link line ahead of
 * libc, so the archive's copy is never pulled in (the same trick
 * printf_float_stub.c uses).
 *
 * Returning from main does not come through here on its own: the startup
 * code ends the program with .EXIT itself rather than calling exit(),
 * which is why main is wrapped below as well, with -Wl,--wrap=main on the
 * link line.  The wrapper is also what makes a normal return flush
 * stdio, since nothing else would.
 *
 * The wording of the marker is DejaGnu's own: it looks for a line saying
 * "*** EXIT code N" in a program's output and takes the number as the
 * status, which is how every board whose machine cannot report one does
 * it.  A program that dies without reaching here prints no such line, and
 * that is read as the failure it is.
 */

#include <stdlib.h>
#include <unistd.h>

extern int __real_main (int, char **);

int
__wrap_main (int argc, char **argv)
{
  /* exit() rather than _exit(): atexit handlers and stdio buffers are
     owed their turn, and on the way out it reaches _exit below anyway.  */
  exit (__real_main (argc, argv));
}

void
_exit (int status)
{
  static const char lead[] = "\n*** EXIT code ";
  char buf[4];
  unsigned int s = (unsigned int) status & 0xff;

  write (1, lead, sizeof lead - 1);
  buf[0] = (char) ('0' + (s / 100) % 10);
  buf[1] = (char) ('0' + (s / 10) % 10);
  buf[2] = (char) ('0' + s % 10);
  buf[3] = '\n';
  write (1, buf, 4);

  /* .EXIT, the same request newlib's own _exit makes.  */
  asm volatile ("emt 0350");
  for (;;)
    ;
}
