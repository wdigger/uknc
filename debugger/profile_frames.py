#!/usr/bin/env python3

"""Run a program under the emulator and save an average-per-frame profile.

The emulator's `monitor frames N` command advances the whole machine by
N video frames (50 Hz).  This wrapper uses that to make repeatable
profiles of animation loops:

    profile_frames.py --start-frame 30 --frames 50 examples/gfour/gfour.elf

It loads the ELF through gdb, skips the requested number of frames,
profiles the requested number of frames, writes the raw `monitor prof
save` output if asked, then writes an averaged profile whose tick counts
are divided by the number of measured frames.  By default it also draws
an SVG report for that average frame.
"""

import argparse
import os
import shutil
import socket
import subprocess
import sys
import time

from prof2cpuprofile import read_profile


FRAME_HZ = 50
CPU_HZ = 8000000
PPU_HZ = 6250000


def here():
    return os.path.dirname(os.path.abspath(__file__))


def root_dir():
    return os.path.dirname(here())


def exe(path):
    if os.path.exists(path):
        return path
    if os.name == 'nt' and os.path.exists(path + '.exe'):
        return path + '.exe'
    return path


def default_tool(path):
    return exe(os.path.join(root_dir(), path))


def free_port():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]
    finally:
        sock.close()


def default_disk(program):
    directory = os.path.dirname(os.path.abspath(program))
    stem = os.path.splitext(os.path.basename(program))[0]
    exact = os.path.join(directory, stem + '.dsk')
    if os.path.exists(exact):
        return exact
    try:
        for name in sorted(os.listdir(directory)):
            if name.lower().endswith('.dsk'):
                return os.path.join(directory, name)
    except OSError:
        pass
    return os.path.join(root_dir(), 'resources', 'rt11os.dsk')


def wait_for_emulator(proc, timeout):
    output = []
    deadline = time.time() + timeout
    while time.time() < deadline:
        line = proc.stdout.readline()
        if line:
            output.append(line)
            if 'Listening on localhost' in line:
                return ''.join(output)
        elif proc.poll() is not None:
            raise RuntimeError('emulator exited before serving gdb:\n' +
                               ''.join(output))
        else:
            time.sleep(0.05)
    raise RuntimeError('emulator did not start serving gdb:\n' +
                       ''.join(output))


def run_checked(cmd, cwd):
    done = subprocess.run(cmd, cwd=cwd, text=True,
                          stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT)
    if done.returncode != 0:
        raise RuntimeError('%s failed with exit code %d:\n%s' %
                           (cmd[0], done.returncode, done.stdout))
    return done.stdout


def write_average(raw, out, frames):
    counts = read_profile(raw)
    if not counts:
        raise RuntimeError('%s has no counted addresses in it' % raw)

    total = sum(ticks for _, ticks in counts)
    with open(out, 'w') as f:
        f.write('# average of %d frame%s at %d Hz\n' %
                (frames, '' if frames == 1 else 's', FRAME_HZ))
        f.write('# raw ticks: %d\n' % total)
        for address, ticks in counts:
            average = int(round(ticks / frames))
            if average > 0:
                f.write('%06o %d\n' % (address, average))
    return total


def main():
    parser = argparse.ArgumentParser(
        description='Run an ELF in the UKNC emulator and profile N frames.')
    parser.add_argument('program', metavar='ELF',
                        help='ELF to load into the machine with gdb')
    parser.add_argument('--start-frame', type=int, default=0,
                        help='frames to run before profiling starts '
                             '(default: 0)')
    parser.add_argument('-n', '--frames', type=int, default=50,
                        help='frames to profile at 50 Hz (default: 50)')
    parser.add_argument('-o', '--out',
                        help='averaged profile to write '
                             '(default: ELF.frame.prof)')
    parser.add_argument('--raw',
                        help='where to keep the raw measured profile '
                             '(default: not kept)')
    parser.add_argument('--svg',
                        help='SVG report to write (default: OUT.svg)')
    parser.add_argument('--no-svg', action='store_true',
                        help='only write the averaged profile')
    parser.add_argument('--by', choices=('line', 'function', 'address'),
                        default='line',
                        help='SVG grouping: source line, function, or address')
    parser.add_argument('--limit', type=int, default=40,
                        help='bars to draw in the SVG (default: 40)')
    parser.add_argument('--processor', choices=('cpu', 'ppu'), default='cpu',
                        help='which emulator profiler to use (default: cpu)')
    parser.add_argument('--start-at', default='main',
                        help='temporary breakpoint to reach before frame '
                             'counting starts (default: main; empty string '
                             'to start counting immediately after load)')
    parser.add_argument('--whole-machine', action='store_true',
                        help='keep addresses the ELF cannot name in the SVG')
    parser.add_argument('--disk',
                        help='disk image to boot (default: a .dsk beside ELF, '
                             'or resources/rt11os.dsk)')
    parser.add_argument('--firmware',
                        default=default_tool('rom/uknc_rom_autoboot.bin'),
                        help='firmware image')
    parser.add_argument('--emulator',
                        default=default_tool('debugger/bin/ukncbtldebug'),
                        help='ukncbtldebug executable')
    parser.add_argument('--gdb',
                        default=default_tool('gcc/bin/pdp11-uknc-rt11-gdb'),
                        help='pdp11-uknc-rt11-gdb executable')
    parser.add_argument('--addr2line',
                        default=default_tool('gcc/bin/pdp11-uknc-rt11-addr2line'),
                        help='addr2line executable for SVG symbolization')
    parser.add_argument('--port', type=int,
                        help='gdbserver port (default: pick a free one)')
    parser.add_argument('--screen', action='store_true',
                        help='show the emulator screen while profiling')
    parser.add_argument('--keep-raw-temp', action='store_true',
                        help='keep the temporary raw profile even without '
                             '--raw, for debugging this script')
    args = parser.parse_args()

    if args.start_frame < 0:
        sys.exit('--start-frame must not be negative')
    if args.frames < 1:
        sys.exit('--frames must be at least 1')

    program = os.path.abspath(args.program)
    if not os.path.exists(program):
        sys.exit('no such ELF: %s' % program)

    disk = os.path.abspath(args.disk) if args.disk else default_disk(program)
    for path, what in ((disk, 'disk'), (args.firmware, 'firmware'),
                       (args.emulator, 'emulator'), (args.gdb, 'gdb')):
        if not os.path.exists(path):
            sys.exit('missing %s: %s' % (what, path))

    base = os.path.splitext(program)[0]
    out = os.path.abspath(args.out) if args.out else base + '.frame.prof'
    svg = None if args.no_svg else (
        os.path.abspath(args.svg) if args.svg
        else os.path.splitext(out)[0] + '.svg')
    raw_keep = os.path.abspath(args.raw) if args.raw else None

    cwd = os.path.dirname(program)
    port = args.port or free_port()
    raw_name = '.%s.raw.prof' % os.path.basename(base)
    raw_path = os.path.join(cwd, raw_name)
    prof = 'prof'
    if args.processor == 'ppu':
        prof += ' ppu'

    emu_cmd = [
        args.emulator,
        '--disk1', disk,
        '--rom', args.firmware,
        '--boot',
        '--port', str(port),
        '--frames', str(max(args.start_frame + args.frames + 100, 1000)),
    ]
    if args.screen:
        emu_cmd.append('--screen')

    emulator = subprocess.Popen(emu_cmd, cwd=cwd, text=True,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT)
    try:
        wait_for_emulator(emulator, 30)

        gdb_cmd = [
            args.gdb, '-batch', '-nx', program,
            '-ex', 'set confirm off',
            '-ex', 'target extended-remote :%d' % port,
            '-ex', 'load',
        ]
        if args.start_at:
            gdb_cmd += [
                '-ex', 'tbreak %s' % args.start_at,
                '-ex', 'continue',
            ]
        if args.start_frame:
            gdb_cmd += ['-ex', 'monitor frames %d' % args.start_frame]
        gdb_cmd += [
            '-ex', 'monitor %s reset' % prof,
            '-ex', 'monitor %s on' % prof,
            '-ex', 'monitor frames %d' % args.frames,
            '-ex', 'monitor %s off' % prof,
            '-ex', 'monitor %s save %s' % (prof, raw_name),
        ]
        gdb_out = run_checked(gdb_cmd, cwd)
    finally:
        emulator.terminate()
        try:
            emulator.wait(timeout=5)
        except subprocess.TimeoutExpired:
            emulator.kill()
            emulator.wait()

    if not os.path.exists(raw_path):
        sys.exit('profile was not saved; gdb said:\n%s' % gdb_out)

    raw_total = write_average(raw_path, out, args.frames)
    if raw_keep:
        shutil.copyfile(raw_path, raw_keep)
    if args.keep_raw_temp or raw_keep:
        kept = raw_keep or raw_path
    else:
        os.unlink(raw_path)
        kept = None

    if svg:
        hz = CPU_HZ if args.processor == 'cpu' else PPU_HZ
        svg_cmd = [
            os.path.join(here(), 'prof2svg.py'),
            '--addr2line', args.addr2line,
            '--by', args.by,
            '--limit', str(args.limit),
            '--hz', str(hz),
            '--title',
            '%s: average of %d frame%s after %d skipped' %
            (os.path.basename(program), args.frames,
             '' if args.frames == 1 else 's', args.start_frame),
            '-e', program,
            '-o', svg,
        ]
        if not args.whole_machine:
            svg_cmd.append('--only-program')
        svg_cmd.append(out)
        run_checked(svg_cmd, cwd)

    avg_total = int(round(raw_total / args.frames))
    print('profiled %d frame%s at %d Hz after skipping %d frame%s' %
          (args.frames, '' if args.frames == 1 else 's', FRAME_HZ,
           args.start_frame, '' if args.start_frame == 1 else 's'))
    print('raw total: %d ticks; average frame: %d ticks' %
          (raw_total, avg_total))
    print('written: %s' % out)
    if kept:
        print('raw: %s' % kept)
    if svg:
        print('svg: %s' % svg)


if __name__ == '__main__':
    try:
        main()
    except RuntimeError as error:
        sys.exit(str(error))
