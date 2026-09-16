// test.js -- check the extension's configuration without the editor.
//
// VS Code cannot be told to press F5 from a script, but the thing it
// would talk to can: cpptools' debug adapter speaks the Debug Adapter
// Protocol over a pipe.  So this builds a launch body with the very
// functions the extension uses, hands it to that adapter, sets a
// breakpoint and reports what comes back.
//
//   node test.js [ПАПКА-ПРОЕКТА [СТРОКА]]
//
// Default is examples/hello in the toolchain this file belongs to.  The
// project must already be built (make <name>.elf).  The line to stop at
// is found in main() when it is not given; give one to stop somewhere
// the program only reaches later -- after it has started the PPU, say.

'use strict';

const cp = require('child_process');
const fs = require('fs');
const path = require('path');

const config = require('./config');

const toolchain = path.dirname(__dirname);
const project = process.argv[2]
      ? path.resolve(process.argv[2])
      : path.join(toolchain, 'examples', 'hello');

// examples/hello builds hello.elf; a project made by the extension
// builds <folder>.elf.  Take whichever is there.
function findProgram(dir) {
    // Same rule as the extension's own findProgram: a .ppu.elf is the
    // PPU side's symbols, not a program to debug.
    const elves = fs.readdirSync(dir).filter(
        (f) => f.endsWith('.elf') && !f.endsWith('.ppu.elf'));
    if (elves.length === 0) {
        console.error('нет .elf в ' + dir + ' -- соберите его сначала');
        process.exit(1);
    }
    return path.join(dir, elves[0]);
}

function findSource(dir) {
    const sources = fs.readdirSync(dir).filter((f) => f.endsWith('.c'));
    return sources.length ? path.join(dir, sources[0]) : null;
}

// A line with code on it, so that the breakpoint is one that can be hit.
// A statement, in other words -- the first line ending in a semicolon.
// Not a brace: the one closing a short function is outside the line
// table, comes back verified and never stops, which looks exactly like a
// broken configuration.
//
// Inside main(), because a statement anywhere else is in a function the
// program may never call -- and a breakpoint that is never reached also
// looks exactly like a broken configuration.
function lineWithCode(source) {
    const lines = fs.readFileSync(source, 'utf8').split('\n');
    let start = lines.findIndex((line) => /^[a-z].*\bmain\s*\(/.test(line));
    if (start < 0) {
        start = 0;
    }
    for (let i = start; i < lines.length; i++) {
        const text = lines[i].trim();
        if (text.endsWith(';') && !text.startsWith('#')) {
            return i + 1;
        }
    }
    return 1;
}

const missing = config.missingParts(toolchain);
if (missing.length > 0) {
    console.error('в тулчейне не хватает: ' + missing.join(', '));
    process.exit(1);
}

const adapter = path.join(
    process.env.HOME, '.vscode', 'extensions');
// By what it holds, not by its name: ms-vscode.cpptools-themes sorts
// after ms-vscode.cpptools and has no adapter in it.
const adapterPath = fs.readdirSync(adapter)
      .filter((d) => d.startsWith('ms-vscode.cpptools-'))
      .sort()
      .map((d) => path.join(adapter, d, 'debugAdapters', 'bin', 'OpenDebugAD7'))
      .filter((f) => fs.existsSync(f))
      .pop();
if (!adapterPath) {
    console.error('расширение ms-vscode.cpptools не установлено');
    process.exit(1);
}
try {
    fs.accessSync(adapterPath, fs.constants.X_OK);
} catch (e) {
    fs.chmodSync(adapterPath, 0o755);
}

const program = findProgram(project);
const source = findSource(project);
const line = process.argv[3]
      ? Number(process.argv[3])
      : (source ? lineWithCode(source) : 1);

const launch = config.buildLaunchConfig({
    toolchain: toolchain,
    program: program,
    cwd: project,
    port: 2345,
});

console.log('тулчейн:  ' + toolchain);
console.log('проект:   ' + project);
console.log('программа:' + path.basename(program));
console.log('останов:  ' + (source ? path.basename(source) : '?') + ':' + line);
console.log('');

//////////////////////////////////////////////////////////////////////

const proc = cp.spawn(adapterPath, [], { stdio: ['pipe', 'pipe', 'pipe'] });
let seq = 0;
const waiting = new Map();
const events = [];
const output = [];

function send(command, args) {
    seq += 1;
    const body = JSON.stringify({ seq, type: 'request', command,
                                  arguments: args });
    proc.stdin.write('Content-Length: ' + Buffer.byteLength(body) +
                     '\r\n\r\n' + body);
    return new Promise((resolve) => waiting.set(seq, resolve));
}

let buffer = Buffer.alloc(0);
proc.stdout.on('data', (chunk) => {
    buffer = Buffer.concat([buffer, chunk]);
    for (;;) {
        const header = buffer.indexOf('\r\n\r\n');
        if (header < 0) {
            return;
        }
        const length = parseInt(
            /Content-Length: (\d+)/.exec(buffer.slice(0, header).toString())[1], 10);
        if (buffer.length < header + 4 + length) {
            return;
        }
        const message = JSON.parse(
            buffer.slice(header + 4, header + 4 + length).toString());
        buffer = buffer.slice(header + 4 + length);

        if (message.type === 'response') {
            const resolve = waiting.get(message.request_seq);
            if (resolve) {
                waiting.delete(message.request_seq);
                resolve(message);
            }
        } else if (message.type === 'event') {
            events.push(message);
            if (message.event === 'output') {
                output.push(message.body.output || '');
            }
        }
    }
});

function waitEvent(name, ms) {
    return new Promise((resolve) => {
        const deadline = Date.now() + ms;
        const tick = () => {
            const found = events.find((e) => e.event === name);
            if (found) {
                return resolve(found);
            }
            if (Date.now() > deadline) {
                return resolve(null);
            }
            setTimeout(tick, 50);
        };
        tick();
    });
}

function report(label, response) {
    const ok = response && response.success;
    console.log(label.padEnd(20) + (ok ? 'ok' : 'ОШИБКА: ' +
                                    (response ? response.message : 'нет ответа')));
    return ok;
}

(async () => {
    let failed = false;

    report('initialize', await send('initialize', {
        clientID: 'test', adapterID: 'cppdbg', linesStartAt1: true,
        columnsStartAt1: true, pathFormat: 'path',
        supportsConfigurationDoneRequest: true,
    }));

    failed |= !report('launch', await send('launch', launch));
    console.log('initialized'.padEnd(20) +
                (await waitEvent('initialized', 30000) ? 'ok' : 'НЕ ПРИШЛО'));

    if (source) {
        const response = await send('setBreakpoints', {
            source: { path: source },
            breakpoints: [{ line: line }],
        });
        if (report('setBreakpoints', response)) {
            const bp = response.body.breakpoints[0];
            console.log('    verified=' + bp.verified + ' line=' + bp.line);
        }
    }

    report('configurationDone', await send('configurationDone', {}));

    const stopped = await waitEvent('stopped', 120000);
    if (!stopped) {
        console.log('stopped'.padEnd(20) + 'НЕ ПРИШЛО');
        failed = true;
    } else {
        console.log('stopped'.padEnd(20) + stopped.body.reason);
        const thread = stopped.body.threadId || 1;
        const stack = await send('stackTrace', { threadId: thread, levels: 3 });
        if (report('stackTrace', stack)) {
            for (const frame of stack.body.stackFrames) {
                console.log('    ' + frame.name + '  ' +
                            ((frame.source || {}).name) + ':' + frame.line);
            }
        }
        await send('continue', { threadId: thread });
        await waitEvent('exited', 60000);
    }

    console.log('');
    console.log('--- вывод программы:');
    process.stdout.write(output.join('').split('\n')
                         .filter((l) => !/^(GNU gdb|Copyright|License|This is free|There is NO|Type |For help|For bug|<http|    <http|This GDB|=|Warning: Debuggee|\d+\+download)/.test(l))
                         .join('\n'));
    console.log('');

    await send('disconnect', { terminateDebuggee: true });
    setTimeout(() => {
        proc.kill();
        process.exit(failed ? 1 : 0);
    }, 1500);
})();
