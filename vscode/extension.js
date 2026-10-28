// extension.js -- the VS Code side.
//
// Press F5 in a folder with a .c file and it brings a machine up, puts
// the program in it and stops where you asked.  No launch.json.
//
// Building belongs to Makefile Tools, which reads the project's own
// Makefile and knows the flags every file is compiled with -- which is
// also where IntelliSense gets them.  What is left here is what that
// extension has no idea about: this machine, and how to run a program on
// it.  F5 still runs make first, because debugging yesterday's build is
// worse than waiting for a no-op.
//
// The debugging itself is cpptools': this extension contributes the
// debug type "УКНЦ" and a provider for it, and the provider answers with
// a cppdbg configuration -- a type VS Code then starts with cpptools'
// own adapter.  That last part is why the type has to change rather than
// stay ours: the adapter looks for a description of its engine named
// after the type it was given (cppdbg.ad7Engine.json), and there is no
// such file for any other name.
//
// See config.js for the configuration, which is where the thinking is.

'use strict';

const vscode = require('vscode');
const cp = require('child_process');
const fs = require('fs');
const net = require('net');
const path = require('path');

const config = require('./config');
const profile = require('./profile');

const CPPTOOLS = 'ms-vscode.cpptools';

let channel = null;

// Somewhere for the extension to say what went wrong, since a provider
// that fails quietly looks exactly like one with nothing to say.
function log(message) {
    if (!channel) {
        channel = vscode.window.createOutputChannel('УКНЦ');
    }
    channel.appendLine(message);
    return channel;
}

// Set by build_extension.sh when the extension is packaged: the
// toolchain it was built from, used when nothing else answers.
let packagedToolchain = '';
try {
    packagedToolchain = require('./packaged.json').toolchain || '';
} catch (e) {
    // Not packaged, or packaged without one.  Fine: the other ways of
    // finding it still apply.
}

//////////////////////////////////////////////////////////////////////

// The folder the project is in.  The file being edited decides, not the
// workspace root: a workspace is often a tree of several programs -- or
// a home directory with the project somewhere below it -- and the one
// on screen is the one meant.
function projectDir(folder) {
    const editor = vscode.window.activeTextEditor;
    if (editor && editor.document.uri.scheme === 'file') {
        return path.dirname(editor.document.uri.fsPath);
    }
    if (folder && folder.uri) {
        return folder.uri.fsPath;
    }
    const first = (vscode.workspace.workspaceFolders || [])[0];
    return first ? first.uri.fsPath : null;
}

function readFile(file) {
    return fs.readFileSync(file, 'utf8');
}

// make, with the toolchain on PATH, waiting for it to finish.  This is
// what makes F5 build first: a preLaunchTask would do it too, but only
// if the project had a tasks.json naming one, and the point here is that
// it needs neither.
function runMake(root, dir, target) {
    return new Promise((resolve) => {
        const bin = path.join(root, 'gcc', 'bin');
        cp.execFile('make', target ? [target] : [], {
            cwd: dir,
            env: Object.assign({}, process.env, {
                PATH: bin + path.delimiter + process.env.PATH,
            }),
        }, (error, stdout, stderr) => {
            resolve({ ok: !error, output: (stdout || '') + (stderr || '') });
        });
    });
}

// Where the toolchain is, or null with the reason already reported.
// Asks once and remembers the answer in the setting.
async function toolchain(folder, okAsk) {
    const setting = vscode.workspace.getConfiguration('uknc')
          .get('toolchainPath', '');
    const found = config.findToolchain({
        setting: setting,
        projectDir: projectDir(folder),
        pathVariable: process.env.PATH,
        packaged: packagedToolchain,
    });
    if (found) {
        return found;
    }

    if (setting) {
        const missing = config.missingParts(setting).join(', ');
        vscode.window.showErrorMessage(
            'УКНЦ: в ' + setting + ' не хватает: ' + missing);
        return null;
    }
    if (!okAsk) {
        return null;
    }

    const picked = await vscode.window.showOpenDialog({
        canSelectFiles: false,
        canSelectFolders: true,
        openLabel: 'Это тулчейн УКНЦ',
        title: 'Где лежит тулчейн УКНЦ?',
    });
    if (!picked || picked.length === 0) {
        return null;
    }
    const root = picked[0].fsPath;
    const missing = config.missingParts(root);
    if (missing.length > 0) {
        vscode.window.showErrorMessage(
            'УКНЦ: в ' + root + ' не хватает: ' + missing.join(', ') +
            '. Соберите тулчейн (gcc/build_gcc_uknc.sh) и эмулятор ' +
            '(debugger/build_debugger_uknc.sh).');
        return null;
    }
    await vscode.workspace.getConfiguration('uknc')
        .update('toolchainPath', root, vscode.ConfigurationTarget.Global);
    return root;
}

// A port nothing is listening on, so that two sessions at once do not
// land on the same one.
function freePort() {
    return new Promise((resolve) => {
        const server = net.createServer();
        server.listen(0, '127.0.0.1', () => {
            const port = server.address().port;
            server.close(() => resolve(port));
        });
        server.on('error', () => resolve(2345));
    });
}

//////////////////////////////////////////////////////////////////////
// Debugging

const debugProvider = {
    // What F5 offers when there is no launch.json at all.
    provideDebugConfigurations() {
        return [{
            type: 'uknc',
            request: 'launch',
            name: 'УКНЦ',
        }];
    },

    async resolveDebugConfiguration(folder, given) {
        if (!vscode.extensions.getExtension(CPPTOOLS)) {
            vscode.window.showErrorMessage(
                'УКНЦ: нужно расширение ' + CPPTOOLS +
                ' — оно ведёт сеанс. Установите: code --install-extension ' +
                CPPTOOLS);
            return undefined;
        }

        const root = await toolchain(folder, true);
        if (!root) {
            return undefined;  // Quietly: toolchain() has said why
        }

        const dir = given.cwd || projectDir(folder);

        // Build first, so that F5 on an edited file runs the edit.  An
        // explicit "program" in launch.json is somebody else's to build.
        if (!given.program && dir) {
            const target = config.elfTarget(dir, readFile);
            const made = await runMake(root, dir, target);
            if (!made.ok) {
                log('make ' + target + ' в ' + dir);
                log(made.output).show(true);
                vscode.window.showErrorMessage(
                    'УКНЦ: не собралось — см. вывод «УКНЦ».');
                return undefined;
            }
        }

        let program = given.program;
        if (!program) {
            const found = config.findProgram(
                dir, (d) => fs.readdirSync(d), (f) => fs.existsSync(f));
            if (found.error) {
                vscode.window.showErrorMessage('УКНЦ: ' + found.error + '.');
                return undefined;
            }
            program = found.program;
        }
        if (!fs.existsSync(program)) {
            vscode.window.showErrorMessage('УКНЦ: нет ' + program + '.');
            return undefined;
        }

        const built = config.buildLaunchConfig({
            toolchain: root,
            program: program,
            cwd: dir || path.dirname(program),
            port: await freePort(),
            name: given.name,
            screen: vscode.workspace.getConfiguration('uknc')
                .get('screen', true),
        });
        return built;
    },
};

//////////////////////////////////////////////////////////////////////
// Tasks

// Running the program on the machine, which is the one thing here that
// is not building and so did not go to Makefile Tools.
async function runOnMachine() {
    const folder = (vscode.workspace.workspaceFolders || [])[0];
    const root = await toolchain(folder, true);
    const dir = projectDir(folder);
    if (!root || !dir) {
        if (root) {
            vscode.window.showErrorMessage(
                'УКНЦ: не понял, что запускать — откройте файл проекта.');
        }
        return;
    }

    const spec = config.runTask(root, dir, readFile);
    const execution = new vscode.ShellExecution(spec.command, {
        cwd: spec.cwd,
        env: { PATH: spec.path + path.delimiter + process.env.PATH },
    });
    const task = new vscode.Task(
        { type: 'uknc', task: spec.label },
        folder || vscode.TaskScope.Workspace,
        spec.label, 'УКНЦ', execution, ['$gcc']);
    task.presentationOptions = { reveal: vscode.TaskRevealKind.Always,
                                 clear: true };
    await vscode.tasks.executeTask(task);
}

//////////////////////////////////////////////////////////////////////
// Profiling
//
// The emulator counts ticks per instruction address; VS Code reads
// .cpuprofile and shows it (see profile.js).  What is left is to put the
// two together without the three commands and the shell in between:
// either over a whole run, or around the piece of a debug session that
// is worth measuring.

function profileSettings() {
    const settings = vscode.workspace.getConfiguration('uknc');
    return {
        by: settings.get('profileBy', 'line'),
        frames: settings.get('profileFrames', 60000),
        // Ticks as microseconds unless asked for real time: the numbers
        // are then proportional and wrong only in their label, which is
        // what one wants while comparing two runs.
        hz: settings.get('profileRealTime', false) ? 8000000 : 0,
        screen: settings.get('screen', true),
    };
}

// profile -> .cpuprofile -> the editor's own viewer.
async function openProfile(root, prof, program) {
    const settings = profileSettings();
    const made = await profile.convertFile({
        prof: prof,
        elf: program,
        by: settings.by,
        hz: settings.hz,
        addr2line: path.join(root, 'gcc', 'bin',
                             'pdp11-uknc-rt11-addr2line'),
    });
    log('профиль: ' + made.ticks + ' такт, ' + made.frames + ' кадров -> ' +
        made.out);
    await vscode.commands.executeCommand(
        'vscode.open', vscode.Uri.file(made.out));
    return made;
}

// "УКНЦ: профилировать прогон" -- the whole program, from load to exit,
// with no breakpoints and nothing to click.
async function profileRun() {
    const folder = (vscode.workspace.workspaceFolders || [])[0];
    const root = await toolchain(folder, true);
    const dir = projectDir(folder);
    if (!root || !dir) {
        if (root) {
            vscode.window.showErrorMessage(
                'УКНЦ: не понял, что профилировать — откройте файл проекта.');
        }
        return;
    }

    // The ELF, because addr2line needs its symbols -- and built first,
    // for the same reason F5 builds: a profile of yesterday's code is
    // worse than waiting.
    const made = await runMake(root, dir, config.elfTarget(dir, readFile));
    if (!made.ok) {
        log(made.output).show(true);
        vscode.window.showErrorMessage('УКНЦ: не собралось — см. вывод «УКНЦ».');
        return;
    }
    const found = config.findProgram(
        dir, (d) => fs.readdirSync(d), (f) => fs.existsSync(f));
    if (found.error) {
        vscode.window.showErrorMessage('УКНЦ: ' + found.error + '.');
        return;
    }

    const settings = profileSettings();
    await vscode.window.withProgress({
        location: vscode.ProgressLocation.Notification,
        title: 'УКНЦ: профилирую ' + path.basename(found.program),
        cancellable: false,
    }, async () => {
        try {
            const run = await profile.runProfile({
                emulator: config.partPath(root, 'emulator'),
                gdb: config.partPath(root, 'gdb'),
                disk: config.partPath(root, 'disk'),
                firmware: config.partPath(root, 'firmware'),
                program: found.program,
                cwd: dir,
                frames: settings.frames,
                screen: settings.screen,
                log: log,
            });
            await openProfile(root, run.prof, found.program);
        } catch (e) {
            log(String(e.message || e)).show(true);
            vscode.window.showErrorMessage('УКНЦ: ' + (e.message || e));
        }
    });
}

// The two halves of measuring a piece of a session rather than a whole
// run.  gdb takes a monitor command when the program is stopped, so
// these belong at a breakpoint: switch the profiler on at one, continue,
// and ask for the profile at the next.
function session() {
    const active = vscode.debug.activeDebugSession;
    if (!active || !active.configuration ||
        active.configuration.miDebuggerServerAddress === undefined) {
        vscode.window.showErrorMessage(
            'УКНЦ: нет сеанса отладки — это для замера части прогона. ' +
            'Для всей программы есть «УКНЦ: профилировать прогон».');
        return null;
    }
    return active;
}

async function monitor(active, command) {
    return active.customRequest('evaluate', {
        expression: '-exec monitor ' + command,
        context: 'repl',
    });
}

async function profileStart() {
    const active = session();
    if (!active) {
        return;
    }
    try {
        await monitor(active, 'prof reset');
        await monitor(active, 'prof on');
        vscode.window.showInformationMessage(
            'УКНЦ: профилировщик включён — продолжите программу, ' +
            'потом «сохранить профиль».');
    } catch (e) {
        vscode.window.showErrorMessage(
            'УКНЦ: не вышло включить профилировщик (программа остановлена?): ' +
            (e.message || e));
    }
}

async function profileShow() {
    const active = session();
    if (!active) {
        return;
    }
    const program = active.configuration.program;
    const dir = active.configuration.cwd || path.dirname(program);
    const prof = path.join(
        dir, path.basename(program).replace(/\.elf$/, '') + '.prof');
    const root = await toolchain(undefined, false);
    if (!root) {
        vscode.window.showErrorMessage('УКНЦ: не нашёл тулчейн.');
        return;
    }
    try {
        await monitor(active, 'prof off');
        // The emulator's own fopen, so the path is its working
        // directory's -- which is the project folder, the session having
        // started it there.
        await monitor(active, 'prof save ' + path.basename(prof));
        await openProfile(root, prof, program);
    } catch (e) {
        log(String(e.message || e)).show(true);
        vscode.window.showErrorMessage('УКНЦ: ' + (e.message || e));
    }
}

// "УКНЦ: открыть профиль" -- a .prof saved by hand, from the debug
// console or from the emulator's own one.
async function profileOpen(uri) {
    const file = uri && uri.fsPath ? uri.fsPath
          : (vscode.window.activeTextEditor
             ? vscode.window.activeTextEditor.document.uri.fsPath : null);
    if (!file || !file.endsWith('.prof')) {
        vscode.window.showErrorMessage(
            'УКНЦ: откройте .prof, сохранённый «monitor prof save».');
        return;
    }
    const dir = path.dirname(file);
    const root = await toolchain(undefined, true);
    if (!root) {
        return;
    }
    const found = config.findProgram(
        dir, (d) => fs.readdirSync(d), (f) => fs.existsSync(f));
    if (found.error) {
        vscode.window.showErrorMessage(
            'УКНЦ: ' + found.error + ' — имена в профиле берутся из .elf.');
        return;
    }
    try {
        await openProfile(root, file, found.program);
    } catch (e) {
        log(String(e.message || e)).show(true);
        vscode.window.showErrorMessage('УКНЦ: ' + (e.message || e));
    }
}

//////////////////////////////////////////////////////////////////////

// "УКНЦ: создать проект" -- a Makefile for a folder that has none, so
// that the tasks and F5 have something to build.  Everything else the
// extension supplies itself.
async function newProject() {
    const folder = (vscode.workspace.workspaceFolders || [])[0];
    const dir = projectDir(folder);
    if (!dir) {
        vscode.window.showErrorMessage('УКНЦ: не открыта папка проекта.');
        return;
    }
    const root = await toolchain(folder, true);
    if (!root) {
        return;
    }

    const makefile = path.join(dir, 'Makefile');
    if (fs.existsSync(makefile)) {
        vscode.window.showWarningMessage('УКНЦ: Makefile уже есть.');
        return;
    }

    const name = path.basename(dir);
    fs.writeFileSync(makefile, [
        '# ' + name + ' для УКНЦ. Написано расширением.',
        '',
        'NAME := ' + name,
        'SRCS := $(wildcard *.c)',
        'OBJS := $(SRCS:.c=.o)',
        '',
        'CC := pdp11-uknc-rt11-gcc',
        '',
        '# -g нужен только .elf, а образу ничего не стоит: отладочные',
        '# секции не ALLOC, .sav выходит байт в байт таким же.',
        'CFLAGS  := -std=gnu23 -fomit-frame-pointer -O2 -g' +
            ' -ffunction-sections -fdata-sections',
        'LDFLAGS := -Wl,--gc-sections',
        '',
        'all:\t$(NAME).dsk',
        '',
        '%.o:\t%.c',
        '\t$(CC) $(CFLAGS) -c -o $@ $<',
        '',
        '# Плоский образ памяти RT-11 — то, что исполняет машина.',
        '$(NAME).sav:\t$(OBJS)',
        '\t$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-Map=$@.map -o $@ $(OBJS)',
        '',
        '# Он же как ELF: тот же код, но с символами и DWARF, которые',
        '# плоскому образу хранить негде. Это читает gdb.',
        '$(NAME).elf:\t$(OBJS)',
        '\t$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-m,pdp11rt11 -o $@ $(OBJS)',
        '',
        '# Загрузочный образ с программой — для запуска без отладчика.',
        '$(NAME).dsk:\t$(NAME).sav',
        '\trm -f $@',
        '\tcp ' + path.join(root, 'resources', 'rt11os.dsk') + ' $@',
        '\trt11dsk a $@ $(NAME).sav',
        '',
        'clean:',
        '\t@-rm -f *.o *.map *.sav *.elf *.dsk',
        '',
        '.PHONY:\tall clean',
        '',
    ].join('\n'));

    const doc = await vscode.workspace.openTextDocument(makefile);
    await vscode.window.showTextDocument(doc);
}

async function showToolchain() {
    const folder = (vscode.workspace.workspaceFolders || [])[0];
    const root = await toolchain(folder, true);
    if (root) {
        vscode.window.showInformationMessage('УКНЦ: тулчейн в ' + root);
    }
}

function activate(context) {
    context.subscriptions.push(
        vscode.debug.registerDebugConfigurationProvider('uknc', debugProvider),
        vscode.commands.registerCommand('uknc.newProject', newProject),
        vscode.commands.registerCommand('uknc.showToolchain', showToolchain),
        vscode.commands.registerCommand('uknc.run', runOnMachine),
        vscode.commands.registerCommand('uknc.profile', profileRun),
        vscode.commands.registerCommand('uknc.profileStart', profileStart),
        vscode.commands.registerCommand('uknc.profileShow', profileShow),
        vscode.commands.registerCommand('uknc.profileOpen', profileOpen));
}

function deactivate() {}

module.exports = { activate, deactivate };
