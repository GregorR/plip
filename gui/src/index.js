import { app, BrowserWindow } from 'electron';
const path = require("path");
const fs = require("fs");

// Handle creating/removing shortcuts on Windows when installing/uninstalling.
if (require('electron-squirrel-startup')) { // eslint-disable-line global-require
  app.quit();
}

const debug = false;

// Handle arguments
const config = (function() {
    let ret = {};
    process.argv.slice(1).forEach((arg) => {
        if (arg[0] === "-") {
            switch (arg) {
                case "-h":
                case "--help":
                    usage();
                    process.exit(0);
                    break;

                default:
                    usage();
                    process.exit(1);
            }

        } else {
            if (ret.file) {
                usage();
                process.exit(1);
            }
            ret.file = arg;

        }
    });
    return ret;
})();

// Usage statement
function usage() {
    process.stderr.write("Usage: plip-gui [input file]\n");
}

// Keep a global reference of the window object, if you don't, the window will
// be closed automatically when the JavaScript object is garbage collected.
let mainWindow;

function createWindow() {
    mainWindow = new BrowserWindow({
        width: debug?1280:640,
        height: 720,
        backgroundColor: "#001",
        webPreferences: {
            nodeIntegration: true
        }
    });

    mainWindow.setMenu(null);

    mainWindow.loadURL(`file://${__dirname}/index.html?c=` + encodeURIComponent(JSON.stringify(config)));

    if (debug) mainWindow.webContents.openDevTools();

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
