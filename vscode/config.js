// config.js -- finding the toolchain, and what to hand the debugger.
//
// Nothing here talks to VS Code, so it can be exercised from a plain
// node script: see test.js beside it, which builds a launch body with
// these functions and runs a real debug session with it.

'use strict';

const fs = require('fs');
const path = require('path');

// What a toolchain folder has to contain to be one.  The compiler alone
// would do to recognise it, but every one of these is needed to debug,
// and saying which is missing is more use than "not a toolchain".
const PARTS = {
    gcc: 'gcc/bin/pdp11-uknc-rt11-gcc',
    gdb: 'gcc/bin/pdp11-uknc-rt11-gdb',
    emulator: 'debugger/bin/ukncbtldebug',
    firmware: 'rom/uknc_rom_autoboot.bin',
    disk: 'resources/rt11os.dsk',
    runner: 'gcc/dejagnu/uknc-run',
};

// libppu's gdb script, which puts the PPU module's own symbols over
// the code running on the PPU (see libs/libppu/ppu.gdb).  Not in PARTS:
// a program with no PPU side never needs it, so a toolchain without it
// is still a toolchain.
const PPU_SCRIPT = 'gcc/xgcc/pdp11-uknc-rt11/lib/ppu.gdb';

function partPath(root, part) {
    return path.join(root, PARTS[part]);
}

// The parts that are not there, in the order above; empty means this is
// a usable toolchain.
function missingParts(root) {
    if (!root) {
        return Object.keys(PARTS);
    }
    return Object.keys(PARTS).filter(
        (part) => !fs.existsSync(partPath(root, part)));
}

function isToolchain(root) {
    return missingParts(root).length === 0;
}

// The toolchain a folder belongs to, if it is inside one: walk up until
// something looks like the root.  This is what makes the extension need
// no setting at all when the project lives among the examples.
function toolchainAbove(dir) {
    let here = dir ? path.resolve(dir) : null;
    while (here) {
        if (isToolchain(here)) {
            return here;
        }
        const up = path.dirname(here);
        if (up === here) {
            return null;
        }
        here = up;
    }
    return null;
}

// The toolchain the compiler on PATH belongs to: .../gcc/bin/pdp11-... ,
// so the root is two directories above the binary.
function toolchainOnPath(pathVariable) {
    const dirs = (pathVariable || '').split(path.delimiter);
    for (const dir of dirs) {
        if (!dir) {
            continue;
        }
        const gcc = path.join(dir, 'pdp11-uknc-rt11-gcc');
        if (fs.existsSync(gcc)) {
            const root = path.dirname(path.dirname(path.dirname(gcc)));
            if (isToolchain(root)) {
                return root;
            }
        }
    }
    return null;
}

// Where the toolchain is, in the order the answers are worth having:
// what the user said, where the project is, what is on PATH, and what
// the extension was packaged with.  Returns null if none of them
// answers.
function findToolchain(options) {
    const o = options || {};
    const candidates = [o.setting, toolchainAbove(o.projectDir),
                        toolchainOnPath(o.pathVariable), o.packaged];
    for (const candidate of candidates) {
        if (candidate && isToolchain(candidate)) {
            return candidate;
        }
    }
    return null;
}

// The body the debug adapter gets.  `spec.screen` false leaves the
// machine's screen out; anything else shows it, which is the default --
// half of what this machine does it does on its screen, and a program
// that draws is not debugged well through a console.  This is the same shape cpptools'
// launch.json takes, because the adapter underneath is cpptools' own:
// gdb drives the session, the emulator is started as its "debug server"
// and is waited for by the line it prints, and the program is put into
// the machine by load once gdb has connected.
//
// The disk image only boots RT-11; the program is not on it.  That is
// what --boot is for, and it is why an edit and F5 is the whole cycle.
// The disk the machine boots from. The system image, unless the project
// has an image of its own beside the program: a program that loads a
// file at run time -- a PPU module, most of all -- needs that file on a
// disk, and its own is where it is. The program itself still comes from
// gdb's load either way, so a stale image costs nothing but its
// contents.
function bootDisk(root, dir, listDir) {
    try {
        const images = listDir(dir).filter((f) => f.endsWith('.dsk'));
        if (images.length > 0) {
            return path.join(dir, images[0]);
        }
    } catch (e) {
        // Unreadable folder: the system image, as for a project with no
        // image of its own.
    }
    return partPath(root, 'disk');
}

function buildLaunchConfig(spec) {
    const root = spec.toolchain;
    const port = spec.port || 2345;
    const dir = spec.cwd || path.dirname(spec.program);
    const disk = bootDisk(root, dir, fs.readdirSync);

    return {
        name: spec.name || 'УКНЦ',
        type: 'cppdbg',
        request: 'launch',

        program: spec.program,
        cwd: dir,
        args: [],

        MIMode: 'gdb',
        miDebuggerPath: partPath(root, 'gdb'),

        // Octal, which is what a PDP-11 is written down in -- every
        // listing, every manual, and this machine's own disassembler.
        // It reaches the debug console, hover and Watch; the Registers
        // pane stays hexadecimal, cpptools asking for that format by
        // name and offering no say in it.
        // The second one teaches gdb about the PPU: sourcing it is
        // enough, since it loads the symbols itself at the first stop
        // after the program starts something on the peripheral
        // processor.  A toolchain built before it existed has no such
        // file, and a program with no PPU side never notices it.
        setupCommands: [
            { text: '-gdb-set output-radix 8', ignoreFailures: false },
            { text: 'source ' + path.join(root, PPU_SCRIPT),
              description: 'символы кода ПП',
              ignoreFailures: true },
        ],

        miDebuggerServerAddress: 'localhost:' + port,

        debugServerPath: partPath(root, 'emulator'),
        debugServerArgs: [
            '--disk1', disk,
            '--rom', partPath(root, 'firmware'),
            '--boot',
            '--port', String(port),
        ].concat(spec.screen === false ? [] : ['--screen']).join(' '),
        serverStarted: 'Listening on localhost',
        filterStdout: true,
        filterStderr: true,

        postRemoteConnectCommands: [
            {
                text: '-target-download',
                description: 'загрузить программу в память машины',
            },
        ],
        // load leaves the program counter at the entry point, so from
        // here it is just "go" -- to the first breakpoint or to the end.
        launchCompleteCommand: 'exec-continue',

        externalConsole: false,
        internalConsoleOptions: 'openOnSessionStart',
    };
}

// The program to debug, looked for rather than worked out from a name.
//
// A folder holds one program here, so the .elf in it is the one -- and
// looking beats guessing, because the name is not always the folder's
// (examples/ppupong builds ping.elf) and the folder is not always the
// project (a workspace root with the project in a subfolder).
//
// Returns { program } or { error }, the error naming where it looked.
function findProgram(dir, listDir, exists) {
    if (!dir) {
        return { error: 'не понял, какую программу отлаживать — откройте файл проекта' };
    }

    let entries;
    try {
        entries = listDir(dir);
    } catch (e) {
        return { error: 'не читается папка ' + dir };
    }

    // .ppu.elf is the peripheral processor's side of the same program
    // -- symbols for gdb to put over the module running on the PPU, not
    // something to debug in its own right (see libs/libppu/ppu.gdb).
    const elves = entries.filter(
        (f) => f.endsWith('.elf') && !f.endsWith('.ppu.elf'));
    if (elves.length === 1) {
        return { program: path.join(dir, elves[0]) };
    }
    if (elves.length > 1) {
        // More than one: the folder's own name decides, and if that is
        // not among them the choice is not ours to make.
        const own = path.basename(dir) + '.elf';
        if (elves.indexOf(own) >= 0) {
            return { program: path.join(dir, own) };
        }
        return {
            error: 'в ' + dir + ' несколько .elf (' + elves.join(', ') +
                ') — какой из них, скажите в "program" в launch.json',
        };
    }

    const sources = entries.filter((f) => f.endsWith('.c'));
    if (sources.length === 0) {
        return { error: 'в ' + dir + ' нет ни .elf, ни .c' };
    }
    if (!exists(path.join(dir, 'Makefile'))) {
        return {
            error: 'в ' + dir + ' нет .elf и нет Makefile — команда ' +
                '«УКНЦ: создать проект» напишет его',
        };
    }
    return {
        error: 'в ' + dir + ' ещё нет .elf — соберите его задачей ' +
            '«Собрать для отладки (ELF)»',
    };
}

// What to ask make for.  The Makefile knows its own target name, and it
// is not always the folder's: examples/ppupong builds ping.elf.  Read it
// out of there, and fall back on the folder's name for a project that
// has no Makefile yet.
function elfTarget(dir, readFile) {
    try {
        const makefile = readFile(path.join(dir, 'Makefile'));
        const explicit = /^([A-Za-z0-9_.-]+)\.elf\s*:/m.exec(makefile);
        if (explicit) {
            return explicit[1] + '.elf';
        }
        // A Makefile built around $(NAME).elf says the name separately.
        const named = /^\s*NAME\s*:?=\s*(\S+)/m.exec(makefile);
        if (named && /\$\(NAME\)\.elf\s*:/.test(makefile)) {
            return named[1] + '.elf';
        }
    } catch (e) {
        // No Makefile, or unreadable: the folder's name is the guess.
    }
    return path.basename(dir) + '.elf';
}

// Running a program on the machine without debugging it: build, then
// hand the image to uknc-run, which boots a machine, runs it and prints
// what it printed.
//
// Building is Makefile Tools' job now, and this is not building -- there
// is no make target for "run it on the UKNC", and there is nothing in
// that extension that knows what a .sav is.
function runTask(root, dir, readFile) {
    const sav = elfTarget(dir, readFile).replace(/\.elf$/, '.sav');
    return {
        label: 'Запустить на машине',
        command: 'make && ' + partPath(root, 'runner') + ' ' + sav,
        cwd: dir,
        path: path.join(root, 'gcc', 'bin'),
    };
}

module.exports = {
    PARTS,
    findProgram,
    elfTarget,
    runTask,
    partPath,
    missingParts,
    isToolchain,
    toolchainAbove,
    toolchainOnPath,
    findToolchain,
    buildLaunchConfig,
};
