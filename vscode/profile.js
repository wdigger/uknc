// profile.js -- the machine's tick profile, and what VS Code can read.
//
// `monitor prof save` writes one "octal address, ticks" line for every
// address that was ever executing: the whole truth about where a
// program's time went, and nothing reads it.  VS Code does read
// .cpuprofile -- the Chrome DevTools / V8 format -- for which it ships a
// table view ordered by self time, and the flame chart comes with this
// extension.  Nothing in that format is specific to JavaScript: it is
// call frames plus samples.
//
// The same conversion lives in debugger/prof2cpuprofile.py, for the
// command line.  It is here again in JavaScript so that looking at a
// profile in the editor needs no Python: the toolchain package ships
// none, and on Windows that is the difference between working and not.
//
// Addresses are named through addr2line out of the ELF the program was
// linked as -- the .sav has no symbols to name them with.
//
// Nothing here talks to VS Code, so it can be run from a plain node
// script.

'use strict';

const cp = require('child_process');
const fs = require('fs');
const net = require('net');
const os = require('os');
const path = require('path');

//////////////////////////////////////////////////////////////////////
// The format the emulator writes

// Returns { counts: [[address, ticks]] } or { error }.
function parseProfile(text, name) {
    const counts = [];
    const lines = text.split('\n');
    for (let i = 0; i < lines.length; i++) {
        const line = lines[i].trim();
        if (!line || line.startsWith('#')) {
            continue;
        }
        const fields = line.split(/\s+/);
        const address = parseInt(fields[0], 8);
        const ticks = parseInt(fields[1], 10);
        if (fields.length !== 2 || !isFinite(address) || !isFinite(ticks)) {
            return {
                error: (name || 'профиль') + ':' + (i + 1) +
                    ': ожидалось «адрес такты», а не «' + line + '»',
            };
        }
        counts.push([address, ticks]);
    }
    return { counts: counts };
}

//////////////////////////////////////////////////////////////////////
// Names

// addr2line for every address at once, as a Map of address to
// { function, file, line }.  One call with the addresses on its
// standard input rather than one call each: there can be thousands of
// them, and a process apiece would take longer than the run being
// profiled.
function symbolize(addresses, spec) {
    return new Promise((resolve, reject) => {
        if (!spec.elf) {
            resolve(new Map());
            return;
        }
        const proc = cp.spawn(spec.addr2line, ['-f', '-e', spec.elf],
                              { stdio: ['pipe', 'pipe', 'pipe'] });
        let out = '';
        let err = '';
        proc.stdout.on('data', (d) => { out += d; });
        proc.stderr.on('data', (d) => { err += d; });
        proc.on('error', (e) => reject(
            new Error('не запускается ' + spec.addr2line + ': ' + e.message)));
        proc.on('close', (code) => {
            if (code !== 0) {
                reject(new Error(spec.addr2line + ': ' + err.trim()));
                return;
            }
            // Two lines per address: the function, then "file:line".
            const fields = out.split('\n');
            const elfDir = path.dirname(path.resolve(spec.elf));
            const where = new Map();
            for (let i = 0; i < addresses.length; i++) {
                const name = (fields[2 * i] || '').trim();
                const source = (fields[2 * i + 1] || '').trim();
                const colon = source.lastIndexOf(':');
                let file = colon < 0 ? '' : source.slice(0, colon);
                // addr2line writes "??" for what it cannot name, and can
                // put a discriminator after the line number.
                let line = parseInt(
                    (colon < 0 ? '' : source.slice(colon + 1)).split(' ')[0],
                    10);
                if (!isFinite(line)) {
                    line = 0;
                }
                if (file === '??') {
                    file = '';
                } else if (file && !path.isAbsolute(file)) {
                    // Relative to where the ELF is, which is where the
                    // build ran for everything this project builds.
                    const beside = path.join(elfDir, file);
                    file = fs.existsSync(beside) ? beside : file;
                }
                where.set(addresses[i], {
                    function: name === '??' ? '' : name,
                    file: file,
                    line: line,
                });
            }
            resolve(where);
        });
        proc.stdin.end(addresses.map(
            (a) => '0x' + (a - (spec.base || 0)).toString(16) + '\n').join(''));
    });
}

//////////////////////////////////////////////////////////////////////
// Frames

function octal(address) {
    return '0' + ('000000' + address.toString(8)).slice(-6);
}

// Ticks summed per frame, heaviest first.  `by` picks what a frame is:
// a source line, a function, or a single address.
function group(counts, where, by) {
    const totals = new Map();
    for (const [address, ticks] of counts) {
        const at = where.get(address) || { function: '', file: '', line: 0 };
        let key;
        if (by === 'address' || !(at.function || at.file)) {
            // One frame per address, either because that is what was
            // asked for or because nothing could name this one.  An
            // address outside the ELF -- the monitor, a driver, the
            // firmware -- has nothing to be grouped by anyway, and
            // throwing all of them together under one name would hide
            // exactly what the profile is read for.
            key = 'a' + address;
        } else if (by === 'function') {
            key = 'f' + at.function + '\0' + at.file;
        } else {
            key = 'l' + at.function + '\0' + at.file + '\0' + at.line;
        }
        const seen = totals.get(key);
        if (seen) {
            seen.ticks += ticks;
            seen.lowest = Math.min(seen.lowest, address);
        } else {
            totals.set(key, { ticks: ticks, lowest: address });
        }
    }

    const frames = [];
    for (const total of totals.values()) {
        const at = where.get(total.lowest) ||
              { function: '', file: '', line: 0 };
        let name;
        if (by === 'address' || !at.function) {
            // The address itself is then the only name the frame can
            // have, and the one to look it up by hand with.
            name = at.function ? octal(total.lowest) + ' ' + at.function
                : octal(total.lowest);
        } else {
            name = at.function;
        }
        frames.push({ ticks: total.ticks, name: name,
                      file: at.file, line: at.line });
    }
    frames.sort((a, b) => (b.ticks - a.ticks) ||
                (a.name < b.name ? -1 : a.name > b.name ? 1 : 0));
    return frames;
}

//////////////////////////////////////////////////////////////////////
// The file VS Code opens

// The viewer gives sample i the delta that follows it -- timeDeltas[i+1]
// -- and the last sample whatever is left of endTime - startTime (see
// buildModel in vscode-js-profile-core, and Chrome's own reader, which
// reads the format the same way).  So one sample per frame, with the
// deltas shifted up by one and the total as the duration, attributes
// every frame exactly its own ticks.
//
// Times go in as microseconds, the format's unit, and one tick becomes
// one microsecond unless `hz` says otherwise: the numbers the viewer
// shows are then ticks under another name, proportional to each other
// and wrong only in their label.  8000000 is the central processor,
// which is the 160000 ticks a frame the profiler counts in, fifty frames
// a second.
function build(frames, hz) {
    const microseconds = (ticks) =>
          hz ? Math.round(ticks * 1e6 / hz) : ticks;

    const nodes = [{
        id: 1,
        callFrame: {
            functionName: '(root)',
            scriptId: '0',
            url: '',
            lineNumber: -1,
            columnNumber: -1,
        },
        hitCount: 0,
        children: frames.map((unused, i) => 2 + i),
    }];
    const samples = [];
    const times = [];
    frames.forEach((frame, i) => {
        nodes.push({
            id: 2 + i,
            callFrame: {
                functionName: frame.name,
                scriptId: '0',
                url: frame.file ? 'file://' + frame.file : '',
                // The format counts lines from zero.
                lineNumber: frame.line > 0 ? frame.line - 1 : -1,
                columnNumber: 0,
            },
            hitCount: frame.ticks,
            children: [],
        });
        samples.push(2 + i);
        times.push(microseconds(frame.ticks));
    });

    return {
        nodes: nodes,
        startTime: 0,
        endTime: times.reduce((a, b) => a + b, 0),
        samples: samples,
        timeDeltas: [0].concat(times.slice(0, -1)),
    };
}

// profile -> .cpuprofile beside it.  Returns { out, frames, ticks } or
// throws with something worth showing.
async function convertFile(spec) {
    const text = fs.readFileSync(spec.prof, 'utf8');
    const parsed = parseProfile(text, path.basename(spec.prof));
    if (parsed.error) {
        throw new Error(parsed.error);
    }
    if (parsed.counts.length === 0) {
        throw new Error('профиль пуст — профилировщик не был включён?');
    }
    const where = await symbolize(parsed.counts.map((c) => c[0]), {
        elf: spec.elf,
        base: spec.base,
        addr2line: spec.addr2line,
    });

    // What the ELF cannot name is not this program: the operating
    // system booting, KMON loading the image, a driver, the firmware.
    // A profile of a whole run has all of that in it -- the machine is
    // started from reset -- and leaving it in would bury the program
    // under the boot.  Dropped by default, kept by onlyProgram false,
    // which is what prof2cpuprofile.py does, it being a converter and
    // not a measurement.
    let counts = parsed.counts;
    if (spec.onlyProgram !== false) {
        counts = counts.filter((c) => {
            const at = where.get(c[0]);
            return at && (at.function || at.file);
        });
        if (counts.length === 0) {
            throw new Error(
                'в профиле нет ни одного адреса из ' +
                path.basename(spec.elf || '') + ' — та ли это программа?');
        }
    }
    const frames = group(counts, where, spec.by || 'line');
    const out = spec.out ||
          spec.prof.replace(/\.prof$/, '') + '.cpuprofile';
    fs.writeFileSync(out, JSON.stringify(build(frames, spec.hz)));
    return {
        out: out,
        frames: frames.length,
        ticks: counts.reduce((sum, c) => sum + c[1], 0),
        all: parsed.counts.reduce((sum, c) => sum + c[1], 0),
    };
}

//////////////////////////////////////////////////////////////////////
// Measuring a whole run

// A port nothing is listening on, so that a profile run and a debug
// session do not land on the same one.
function freePort() {
    return new Promise((resolve) => {
        const server = net.createServer();
        server.listen(0, '127.0.0.1', () => {
            const port = server.address().port;
            server.close(() => resolve(port));
        });
        server.on('error', () => resolve(31937));
    });
}

// The program, start to finish, with the profiler on for all of it.
//
// The same arrangement uknc-run uses, because it is the one that ends by
// itself: the image goes onto a copy of the boot disk under a name RT-11
// will take, the emulator does nothing but serve gdb, and "run" is what
// boots the machine and starts the program -- the server resets, waits
// for RT-11 and types "R T", and the run ends when the program does.
// Nothing here has to be timed from outside, and nothing has to know
// what the program prints.
//
// The boot is inside the measurement, there being no moment between it
// and the program to switch the profiler on at.  That is what
// convertFile's onlyProgram is for: the operating system's addresses are
// not in the ELF, and what the ELF cannot name is dropped.
//
// --frames is the one guard: a program that never exits would otherwise
// keep the editor waiting for ever, and the emulator counts its own time
// in frames of a fiftieth of a second whatever the host is doing.
//
// Returns { prof, output } or throws.
async function runProfile(spec) {
    const port = spec.port || await freePort();
    const name = path.basename(spec.program).replace(/\.elf$/, '');
    const work = fs.mkdtempSync(path.join(
        fs.realpathSync(os.tmpdir()), 'uknc-prof-'));
    let output = '';
    const say = (text) => {
        output += text;
        if (spec.log) {
            spec.log(String(text).replace(/\n$/, ''));
        }
    };

    const run = (program, args, cwd) => new Promise((resolve, reject) => {
        const proc = cp.spawn(program, args, { cwd: cwd,
                                               stdio: ['ignore', 'pipe', 'pipe'] });
        proc.stdout.on('data', say);
        proc.stderr.on('data', say);
        proc.on('error', (e) => reject(
            new Error('не запускается ' + program + ': ' + e.message)));
        proc.on('exit', (code) => resolve(code));
    });

    try {
        // RT-11 names are 6.3 and upper case, so on the disk the program
        // is T.SAV whatever it is called here.  rt11dsk takes the name it
        // is given as the name on the disk, and a path is not a 6.3 name
        // -- hence bare names from inside the working directory.
        fs.copyFileSync(spec.sav, path.join(work, 'T.SAV'));
        fs.copyFileSync(spec.disk, path.join(work, 't.dsk'));
        const put = await run(spec.rt11dsk, ['a', 't.dsk', 'T.SAV'], work);
        if (put !== 0) {
            throw new Error('rt11dsk не положил программу на диск');
        }

        const emulator = cp.spawn(spec.emulator, [
            '--disk1', 't.dsk',
            '--rom', spec.firmware,
            '--port', String(port),
            '--frames', String(spec.frames || 60000),
        ].concat(spec.screen ? ['--screen'] : []),
            { cwd: work, stdio: ['ignore', 'pipe', 'pipe'] });
        emulator.stdout.on('data', say);
        emulator.stderr.on('data', say);

        try {
            await new Promise((resolve, reject) => {
                const timer = setTimeout(
                    () => reject(new Error('эмулятор не отозвался')), 30000);
                emulator.stdout.on('data', (d) => {
                    if (String(d).indexOf('Listening on localhost') >= 0) {
                        clearTimeout(timer);
                        resolve();
                    }
                });
                emulator.on('exit', (code) => {
                    clearTimeout(timer);
                    reject(new Error('эмулятор вышел с кодом ' + code));
                });
            });

            // The ELF on the command line is for the symbols gdb prints
            // with; what runs is the .sav on the disk.
            await run(spec.gdb, [
                '-batch', '-nx', spec.program,
                '-ex', 'set confirm off',
                '-ex', 'target extended-remote :' + port,
                '-ex', 'set remote exec-file T',
                '-ex', 'monitor prof reset',
                '-ex', 'monitor prof on',
                '-ex', 'run',
                '-ex', 'monitor prof off',
                '-ex', 'monitor prof save prof.txt',
            ], work);
        } finally {
            emulator.kill();
        }

        const written = path.join(work, 'prof.txt');
        if (!fs.existsSync(written)) {
            throw new Error('профиль не сохранился — см. вывод «УКНЦ»');
        }
        // Kept beside the program, where the one saved by hand goes too.
        const prof = path.join(spec.cwd, name + '.prof');
        fs.copyFileSync(written, prof);
        return { prof: prof, output: output };
    } finally {
        fs.rmSync(work, { recursive: true, force: true });
    }
}

module.exports = {
    parseProfile,
    symbolize,
    group,
    build,
    convertFile,
    runProfile,
    freePort,
};
