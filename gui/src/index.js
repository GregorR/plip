import { app, BrowserWindow } from 'electron';
import { addBypassChecker } from 'electron-compile';
const path = require("path");
const fs = require("fs");

// Bypass compilation for files outside the app
addBypassChecker((filePath) => {
    return filePath.indexOf(app.getAppPath()) !== 0;
});

// Handle creating/removing shortcuts on Windows when installing/uninstalling.
if (require('electron-squirrel-startup')) { // eslint-disable-line global-require
  app.quit();
}

const debug = false;

// Handle arguments
const config = (function() {
    let ret = {};
    let escaped = false;
    process.argv.slice(1).forEach((arg) => {
        if (arg[0] === "-" && !escaped) {
            switch (arg) {
                case "-h":
                case "--help":
                    usage();
                    process.exit(0);
                    break;

                case "-e":
                case "--edit":
                    ret.edit = true;
                    break;

                case "--":
                    escaped = true;
                    break;

                default:
                    usage();
                    process.exit(1);
            }

        } else {
            if (ret.edit) {
                if (!ret.media) {
                    ret.media = arg;
                } else if (!ret.waveform) {
                    ret.waveform = arg;
                } else if (!ret.inMarks) {
                    ret.inMarks = arg;
                } else if (!ret.outMarks) {
                    ret.outMarks = arg;
                } else {
                    usage();
                    process.exit(1);
                }

            } else {
                if (ret.file) {
                    usage();
                    process.exit(1);
                }

                // electron-forge uses this argument as cwd, so ignore it
                try {
                    let sbuf = fs.statSync(arg);
                    if (!sbuf.isDirectory())
                        ret.file = arg;
                } catch (ex) {}

            }

        }
    });

    if (ret.edit) {
        if (!ret.inMarks) {
            usage();
            process.exit(1);
        }
        if (!ret.outMarks)
            ret.outMarks = ret.inMarks;
    }

    return ret;
})();

// Usage statement
function usage() {
    process.stderr.write(
        "Usage: plip-gui [input file]\n" +
        "Or: plip-gui <-e|--edit> <media file> <waveform file> <input marks> [output marks]\n\n");
}

// Keep a global reference of the window object, if you don't, the window will
// be closed automatically when the JavaScript object is garbage collected.
let mainWindow;

function createWindow() {
    mainWindow = new BrowserWindow({
        width: config.edit?1600:(debug?1280:640),
        height: config.edit?900:720,
        backgroundColor: "#001",
        webPreferences: {
            nodeIntegration: true
        }
    });

    mainWindow.setMenu(null);

    let base = "index.html";
    if (config.edit)
        base = "medit.html";
    mainWindow.loadURL(`file://${__dirname}/${base}?c=` + encodeURIComponent(JSON.stringify(config)));

    if (debug) mainWindow.webContents.openDevTools();

    if (config.edit)
        mainWindow.maximize();

    mainWindow.on('closed', () => {
        mainWindow = null;
    });
};

// This method will be called when Electron has finished
// initialization and is ready to create browser windows.
// Some APIs can only be used after this event occurs.
app.on('ready', createWindow);

app.on('window-all-closed', () => {
    app.quit();
});

app.on('activate', () => {
    if (mainWindow === null) {
        createWindow();
    }
});
