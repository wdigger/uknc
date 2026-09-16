# ppu.gdb -- see the PPU's own code in the debugger.
#
#   source .../lib/ppu.gdb
#
# The emulator serves both processors, so the peripheral one's
# registers and memory are there for the asking -- as inferior 2 when
# gdb asked for the multiprocess extension (which it does by default),
# or as thread 2 of one inferior when it did not.  Its symbols are
# another matter: gdb reads one executable, and that is the central
# processor's program.  A PPU module is loaded by that program itself,
# at an address chosen at run time, from a .PPU file which is an RT-11
# object module rather than ELF.
#
# This puts the right symbols there.  Build the PPU side a second time
# as ELF beside its .PPU --
#
#   pdp11-uknc-rt11-ld-ppu --elf -o foo.ppu.elf foo.o
#
# -- and that file's symbols, DWARF and all, are placed over the
# running module, so a breakpoint by function name, a backtrace and a
# source listing all work on the PPU side.  The address they go at is
# the one libppu's ppuc_run() recorded in ppuc_code_base (see
# ppu_client.h): where the module ended up.
#
# Sourcing this arms it: at every stop, if the program has started
# something on the PPU and a .ppu.elf is lying beside the executable,
# the symbols load themselves.  "ppu-symbols FILE" does it on demand
# (and says why if it cannot), "ppu-symbols-auto off" stops it
# happening by itself.
#
# One thing about the machine, while gdb is being told about it: both
# processors run whenever it runs, and with two inferiors gdb's own
# default is to resume only the one in front -- and then a breakpoint
# the other reaches arrives as a signal out of nowhere, since as far as
# gdb knows that process was not even running.  This says what is
# actually so.
set schedule-multiple on

# Two inferiors or two threads decides where the symbols go, and that
# is the whole difference between the two: a program space is per
# inferior, so with the multiprocess extension the PPU's symbols are
# the PPU's alone -- and the central processor's stay out of its
# backtraces.  Sharing one, they land in the same table as the CPU
# program's, and wherever the module overlaps that program's own
# addresses (the usual case), which of the two gdb shows for an address
# is not something this can decide.

python

import os
import glob
import gdb


# The two processors are numbered the same either way: 1 is the central
# one, 2 the peripheral one (see the emulator's GdbServer.cpp).  What
# differs is whether that number names an inferior or a thread.
def _inferior(num):
    for inferior in gdb.inferiors():
        if inferior.num == num:
            return inferior
    return None


def _separate_inferiors():
    return _inferior(2) is not None


# What the user had selected, to put back afterwards: nothing here is
# something they asked for, so nothing here should leave a mark.
class _Selection(object):
    def __init__(self):
        self.thread = gdb.selected_thread()

    def restore(self):
        if self.thread is not None and self.thread.is_valid():
            self.thread.switch()


# Selecting a thread selects its inferior with it -- and the program
# space that goes with that -- which is the whole of what is needed
# here, either way round.  Through the thread rather than the "inferior"
# command on purpose: that command announces itself, and this happens
# at every stop, behind the user's back.
def _select(num):
    if _separate_inferiors():
        inferior = _inferior(num)
        threads = inferior.threads() if inferior else ()
        if not threads:
            return False
        threads[0].switch()
        return True

    for thread in gdb.selected_inferior().threads():
        if thread.num == num:
            thread.switch()
            return True
    return False


# ppuc_code_base is a CPU-side variable: read as the PPU, that address
# would come out of the PPU's own, entirely different memory.
#
# Not before the program is running, either.  Until then the variable's
# memory still holds whatever the machine had there, and a number read
# out of it is somebody else's -- symbols placed at it look perfectly
# consistent and are wrong, which is worse than none at all.  The
# program counter says: once it is inside the executable, the program
# has been loaded and started, and crt0 has zeroed .bss (so the answer
# is 0 until ppuc_run() puts the real address there).
def _read_base():
    selection = _Selection()
    try:
        if not _select(1):
            return None
        pc = int(gdb.parse_and_eval('$pc'))
        if ' in section ' not in gdb.execute('info symbol %d' % pc,
                                             to_string=True):
            return None
        # Through the address, because libppu is compiled without debug
        # information: with no type to read it by, the name alone --
        # and a cast of it -- gives gdb's answer for where the variable
        # is, not what is in it.  Harmless if it ever does have a type.
        return int(gdb.parse_and_eval('*(unsigned short *)&ppuc_code_base'))
    except gdb.error:
        return None            # No libppu in this program
    finally:
        selection.restore()


# The one .ppu.elf beside the executable, if there is exactly one.
def _find_elf():
    progspace = gdb.current_progspace()
    filename = progspace.filename if progspace else None
    if not filename:
        # The PPU's own program space has no executable of its own;
        # the file to look beside is the CPU program's.
        for objfile_space in gdb.progspaces():
            if objfile_space.filename:
                filename = objfile_space.filename
                break
    if not filename:
        return None
    found = glob.glob(os.path.join(os.path.dirname(filename), '*.ppu.elf'))
    return found[0] if len(found) == 1 else None


class _PpuSymbols(object):
    def __init__(self):
        self.filename = None
        self.base = None
        self.automatic = True

    # Returns None when the symbols are (already) in place, or the
    # reason they are not.  Loading twice at the same address would
    # only duplicate them, so a repeat is not an error.
    def load(self, filename=None):
        filename = filename or self.filename or _find_elf()
        if not filename:
            return ('no .ppu.elf beside the executable -- build one with '
                    'pdp11-uknc-rt11-ld-ppu --elf')
        if not os.path.exists(filename):
            return 'no such file: ' + filename

        base = _read_base()
        if base is None:
            return ('nothing to go by: either the program is not running '
                    'yet, or it has no ppuc_code_base -- that is, it does '
                    'not use libppu')
        if base == 0:
            return 'nothing is running on the PPU yet (ppuc_code_base is 0)'
        if self.base == base and self.filename == filename:
            return None

        selection = _Selection()
        try:
            # Into the PPU's own program space, where a symbol table of
            # its own can exist; with one shared space this is the same
            # table as the CPU program's and the switch changes nothing.
            _select(2)
            if self.base is not None:
                # A second ppuc_run(), at a different address: the old
                # symbols now describe nothing.
                try:
                    gdb.execute('remove-symbol-file ' + self.filename,
                                to_string=True)
                except gdb.error:
                    pass
            gdb.execute('add-symbol-file %s -o %d' % (filename, base),
                        to_string=True)
        finally:
            selection.restore()

        self.filename, self.base = filename, base
        gdb.write('PPU symbols from %s at 0%o\n'
                  % (os.path.basename(filename), base))
        return None

    # Every stop is a chance: the PPU is started by the program itself,
    # so there is no one moment to hook, and this costs one word of
    # memory read per stop.  Every stop, not just the first: a program
    # that runs a second module, or is restarted, loads it somewhere
    # else, and load() replaces the symbols when the address changes.
    def on_stop(self, event):
        if self.automatic:
            self.load()


_ppu_symbols = _PpuSymbols()
gdb.events.stop.connect(_ppu_symbols.on_stop)


class _PpuSymbolsCommand(gdb.Command):
    """Load the PPU module's symbols over the code running on the PPU.

Usage: ppu-symbols [FILE]

FILE is the PPU side linked as ELF (pdp11-uknc-rt11-ld-ppu --elf);
without it, the one .ppu.elf beside the executable is used."""

    def __init__(self):
        super(_PpuSymbolsCommand, self).__init__('ppu-symbols',
                                                 gdb.COMMAND_FILES)

    def invoke(self, argument, from_tty):
        self.dont_repeat()
        problem = _ppu_symbols.load(argument.strip() or None)
        if problem:
            raise gdb.GdbError(problem)


class _PpuSymbolsAutoCommand(gdb.Command):
    """Whether PPU symbols load by themselves at the first stop after
the PPU is started.  On by default.

Usage: ppu-symbols-auto [on|off|FILE]"""

    def __init__(self):
        super(_PpuSymbolsAutoCommand, self).__init__('ppu-symbols-auto',
                                                     gdb.COMMAND_FILES)

    def invoke(self, argument, from_tty):
        self.dont_repeat()
        argument = argument.strip()
        if argument == 'off':
            _ppu_symbols.automatic = False
        elif argument in ('', 'on'):
            _ppu_symbols.automatic = True
        else:
            _ppu_symbols.filename = argument
            _ppu_symbols.automatic = True
        gdb.write('PPU symbols load automatically: %s\n'
                  % ('yes' if _ppu_symbols.automatic else 'no'))


_PpuSymbolsCommand()
_PpuSymbolsAutoCommand()

end
