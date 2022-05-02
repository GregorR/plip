/*
 * Copyright (c) 2020-2022 Gregor Richards
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

(function() {
    const cp = require("child_process");
    const fs = require("fs");
    const path = require("path");
    const electron = require("electron");
    const dialog = electron.remote.dialog;
    const BrowserWindow = electron.remote.BrowserWindow;

    const IniFile = require("./plipini.js").IniFile;

    const url = new URL(window.location);
    const urlParams = new URLSearchParams(url.search);
    const configText = urlParams.get("c");
    const config = configText ? JSON.parse(configText) : {};

    const gebi = document.getElementById.bind(document);
    const ce = document.createElement.bind(document);

    const spinner = gebi("spinner");
    const outputBox = gebi("outputBox");
    const outputBoxClose = gebi("outputBoxClose");
    const details = gebi("details");
    const editorBox = gebi("editorBox");

    const tracks = gebi("tracks");
    const outdir = gebi("outdir");

    const crRE = /^(.*)\r([^\r]*)$/;

    // By default we do everything
    const defSteps = "dpecm";

    // Our default final mix options
    const defOptions = "-c:v libx264 -c:a aac -crf 20 -threads 0 -b:a 128k";

    // The path to the file we're reading
    let filePath = null;

    // The path to the directory to do our work in, or null if it's unset
    let dirPath = null;

    // All the streams we've detected
    let streams = null;

    // The relevant config file
    let configFileINI = null;

    // The steps the GUI should perform
    const allSteps = ["d", "p", "e", "c", "m"];
    let steps = null;

    // The formats, extracted from the configuration file
    let formats = null;

    // Hunt for the path to plip's tools
    let binPathParts = path.dirname(process.execPath).split(path.sep);
    while (binPathParts.length) {
        let binPath = binPathParts.join(path.sep) + path.sep + "bin";
        let plipPath = binPath + path.sep + "plip-demux" + ((path.sep==="\\")?".exe":"");
        try {
            fs.accessSync(plipPath);
            process.stderr.write(`plip path: ${binPath}\n`);
            process.env.PATH = binPath + path.delimiter + process.env.PATH;
            break;
        } catch (ex) {}
        binPathParts.pop();
    }

    // Hide all "subwindows"
    function hideAll() {
        spinner.style.display =
            details.style.display =
            editorBox.style.display = "none";
    }

    // Show the given "subwindow"
    function showOnly(x) {
        hideAll();
        x.style.display = "block";
    }

    // Show the spinner, regardless of all else
    function spin() {
        spinner.style.display = "block";
    }

    function unspin() {
        spinner.style.display = "none";
    }

    // When we change our input file, we totally reset
    gebi("input").onchange = function() {
        if (this.files[0]) {
            loadFile(this.files[0].path);
        } else {
            filePath = null;
            hideAll();
        }
    }

    // Load a file
    function loadFile(file) {
        filePath = file;
        dirPath = path.dirname(filePath);
        outdir.value = "";
        showOnly(spinner);

        loadConfig();
        detectTracks();
    }

    // When we change our output directory, our configuration files might be affected
    outdir.onchange = function() {
        if (this.files[0])
            dirPath = this.files[0].path;
        else
            dirPath = path.dirname(filePath);

        loadConfig();
        detectTracks();
    }

    // Config file selection
    const configSelect = gebi("config");
    const configFileBox = gebi("configFileBox");
    const configFileInput = gebi("configFile");
    configSelect.onchange = function() {
        let configFileBox = gebi("configFileBox");
        if (this.value === "select")
            configFileBox.style.display = "block";
        else
            configFileBox.style.display = "none";

        loadConfig();
        detectTracks();
    }

    configFileInput.onchange = function() {
        loadConfig();
        detectTracks();
    }

    // Get the full path of our configuration file, or null if it's the default
    function configFile() {
        switch (configSelect.value) {
            case "select":
                if (configFileInput.files[0])
                    return configFileInput.files[0].path;
                return null;

            default:
                return null;
        }
    }

    /* We load our configuration file directly here, as well as updating the
     * steps box since the steps to perform are saved in the config file */
    function loadConfig() {
        let file = configFile();
        if (file === null)
            file = dirPath + path.sep + "plip.ini";
        configFileINI = new IniFile(null, file);

        // Automatically load our steps
        loadSteps();

        // And our formats
        loadFormats();

        // And our options
        let mob = gebi("mixOptions");
        mob.onchange = null;
        let configMixOpt = configFileINI.getValue("", "gui", "mixoptions");
        if (configMixOpt !== "")
            mob.innerText = configMixOpt;
        else
            mob.innerText = defOptions;
        mob.onchange = mixOptionsChange;
    }

    // Load the steps from the config file
    function loadSteps() {
        let stepStr = configFileINI.getValue("", "gui", "steps");
        if (stepStr === "")
            stepStr = defSteps;

        // Split it into the individual steps
        steps = {length: 0};
        allSteps.forEach((step) => {
            if (stepStr.includes(step)) {
                steps[step] = true;
                steps.length++;
            }
        });

        // And display that state
        displaySteps();
    }

    // Load the formats and programs from the config file
    function loadFormats() {
        formats = {};
        [
            ["vformat", "mkv"],
            ["aiformat", "flac"],
            ["aformat", "wav"]
        ].forEach((formatPair) => {
            let format = formatPair[0];
            let def = formatPair[1];
            formats[format] = configFileINI.getValue("", "formats", format) || def;
        });

        ["ffmpeg", "audiowaveform"].forEach((program) => {
            formats[program] = configFileINI.getValue("", "programs", program) || program;
        });
    }

    // Display the steps which are en/disabled, with buttons to change it
    function displaySteps() {
        const stepsBox = gebi("steps");
        const stepNames = {
            "d": "Demux",
            "p": "Process",
            "e": "Edit",
            "c": "Clip",
            "m": "Mix"
        };

        const tr = ce("tr");
        stepsBox.innerHTML = "";
        stepsBox.appendChild(tr);

        allSteps.forEach((step) => {
            let nm = stepNames[step];
            let td = ce("td");
            tr.appendChild(td);
            let b = ce("button");
            td.appendChild(b);
            let inc = steps[step];
            b.style.color = "#fff";
            b.style.backgroundColor = inc?"#030":"#300";
            b.innerText = stepNames[step] + " " + (inc?"✓":"✗");

            // Now make it change when we click it
            if (step !== "e") {
                b.onclick = function() {
                    let newSteps = "";
                    for (let i = 0; i < allSteps.length; i++) {
                        let newStep = allSteps[i];
                        if (newStep === "e") {
                            if (steps.e)
                                newSteps += "e";
                        } else {
                            newSteps += newStep;
                        }
                        if (newStep === step)
                            break;
                    }

                    updateSteps(newSteps);
                };

            } else { // Edit step
                b.onclick = function() {
                    let newSteps = "";
                    for (let i = 0; i < allSteps.length; i++) {
                        let newStep = allSteps[i];
                        if (newStep === "e") {
                            if (!steps.e && steps.p)
                                newSteps += newStep;
                        } else {
                            if (steps[newStep])
                                newSteps += newStep;
                        }
                    }

                    updateSteps(newSteps);
                };

            }

        });

        // Update the steps when a button is clicked
        function updateSteps(newSteps) {
            // Now write it to the config file
            configFileINI.update(null, "gui", "steps", newSteps);
            configFileINI.save((err) => {
                if (err) alert("WARNING: Failed to save configuration file!");
                loadConfig();
            });
        }

    }

    gebi("editButton").onclick = function() {
        gebi("editor").value = configFileINI.serialize();
        showOnly(editorBox);
    }

    gebi("configSave").onclick = function() {
        spin();
        fs.writeFile(configFileINI.file, gebi("editor").value, (err) => {
            if (err) alert("WARNING: Failed to write the configuration file!");
            loadConfig();
            detectTracks();
        });
    }

    gebi("configCancel").onclick = function() {
        showOnly(details);
    }

    function mixOptionsChange() {
        configFileINI.update(null, "gui", "mixoptions", gebi("mixOptions").innerText);
        configFileINI.save();
    }

    // Detect our tracks and their status
    function detectTracks() {
        return new Promise(function(res, rej) {
            let infoRaw = "";
            const trackRE = /^\^PLIP: (Video|Audio) track (.*) \(([0-9]*)\) (included|excluded)$/;

            let args = ["-n", filePath];
            let cf = configFile();
            if (cf) {
                args.push("-c");
                args.push(cf);
            }

            let p = cp.spawn("plip-demux", args, {
                cwd: dirPath,
                stdio: ["ignore", "ignore", "pipe"],
                windowsHide: true
            });

            p.stderr.on("data", (chunk) => {
                infoRaw += chunk;
            });

            p.stderr.on("end", () => {
                streams = [];

                if (infoRaw === "") {
                    alert("plip-demux not found! Is plip installed?");
                    hideAll();
                    rej();
                    return;
                }

                infoRaw.split("\n").forEach((line) => {
                    line = line.trim();
                    line = trackRE.exec(line);
                    if (!line) return;

                    streams.push({
                        type: line[1],
                        title: line[2],
                        idx: line[3],
                        included: line[4]
                    });
                });

                if (streams.length === 0) {
                    alert("This doesn't appear to be a media file!");
                    hideAll();
                    rej();
                    return;
                }

                tracks.innerHTML = "";
                listTracks(streams, "Video");
                listTracks(streams, "Audio");

                showOnly(details);

                res();
            });
        });
    }

    function listTracks(infoTracks, type) {
        // Figure out if we have any streams of the given type
        var ct = 0;
        infoTracks.forEach((track) => {
            if (track.type === type)
                ct++;
        });

        if (ct) {
            let block = ce("div");
            let h = ce("h3");
            h.innerText = type;
            block.appendChild(h);

            // Begin our display
            let table = ce("table");
            let tbody = ce("tbody");
            let tr = ce("tr");
            let th = ce("th");
            th.innerText = "Track";
            th.style.minWidth = "7em";
            tr.appendChild(th);
            th = ce("th");
            th.innerText = "Include?";
            tr.appendChild(th);
            th = ce("th");
            th.innerText = "Demuxed?";
            tr.appendChild(th);
            if (type !== "Video") {
                th = ce("th");
                th.innerText = "Audio processed?";
                tr.appendChild(th);
            }
            th = ce("th");
            th.innerText = "Clipped?";
            tr.appendChild(th);
            tbody.appendChild(tr);

            function appendIndicator(truth, files, buttony) {
                let td = ce("td");
                let rep = td;

                td.style.textAlign = "center";

                function setTruth(to) {
                    let color = "#310";
                    let text = "...";
                    if (typeof to === "boolean") {
                        color = to?"#030":"#300";
                        text = to?"✓":"✗";
                    }
                    rep.style.backgroundColor = color;
                    rep.innerText = text;
                }

                // Set up the interface
                if (buttony) {
                    rep = ce("button");
                    rep.style.width = "80%";
                    rep.style.color = "#ddd";
                    td.appendChild(rep);
                }

                // Set truth by the initial value
                setTruth(files?null:truth);

                // Update it as necessary
                if (files) {
                    function checkFiles() {
                        let file = files.shift();
                        fs.access(file, (err) => {
                            if (err) {
                                if (files.length) {
                                    checkFiles();
                                } else {
                                    setTruth(false);
                                }
                            } else {
                                setTruth(true);
                            }
                        });
                    }
                    checkFiles();
                }

                tr.appendChild(td);
                return td;
            }

            // Make a row for each track
            infoTracks.forEach((t) => {
                if (!t.title) return;
                if (t.type !== type) return;
                tr = ce("tr");

                let td = ce("th");
                td.innerText = t.title;
                tr.appendChild(td);

                // The included indicator is a button, by which we can change the inclusion
                let ibut = appendIndicator(t.included==="included", null, true);
                ibut.onclick = function() {
                    // Set this explicitly to be included/excluded
                    spin();
                    configFileINI.update(t.title, "tracks", "include", (t.included==="included")?"n":"y");
                    configFileINI.save((err) => {
                        if (err) alert("WARNING: Failed to save configuration file!");
                        detectTracks();
                    });
                }

                // Figure out all our filenames
                let clipTitle = t.title;
                let base = dirPath + path.sep + t.title;
                let demuxed = base + ((type==="Video")?".track":("-raw." + formats.aiformat));
                let processed = base + "-proc." + formats.aiformat;
                let clipBase = dirPath + path.sep + clipTitle;
                let clipFormat = (type==="Video")?formats.vformat:formats.aformat;
                let clipped = clipBase + "." + clipFormat;
                let clipped1 = clipBase + "1.mkv" + clipFormat;

                if (type === "Video") {
                    appendIndicator(false, [demuxed, clipped, clipped1]);
                    appendIndicator(false, [clipped, clipped1]);
                } else {
                    appendIndicator(false, [demuxed, processed, clipped, clipped1]);
                    appendIndicator(false, [processed, clipped, clipped1]);
                    appendIndicator(false, [clipped, clipped1]);
                }

                tbody.appendChild(tr);
            });
            table.appendChild(tbody);
            block.appendChild(table);
            tracks.appendChild(block);
        }
    }

    gebi("process").onclick = processing;

    outputBoxClose.onclick = function() {
        outputBox.style.display = "none";
    };

    async function processing() {
        // Figure out the output file if we need to
        let mixPath = null;
        if (steps.m) {
            mixPath = await dialog.showSaveDialog({
                defaultPath: filePath + "-proc.mkv",
                filters: [
                    {name: "Matroska file", extensions: ["mkv"]},
                    {name: "MP4 file", extensions: ["mp4"]}
                ],
                properties: ["showOverwriteConfirmation"]
            });
            if (!mixPath) return;
        }

        // Set up the UI
        gebi("process").disabled = true;
        outputBox.style.display = "block";
        outputBoxClose.style.display = "none";
        outputBox.scrollIntoView();

        // Figure out our expected output lines
        let demuxCt = 1, clipCt = 0;
        streams.forEach((stream) => {
            if (stream.type === "Audio") {
                demuxCt++;
                if (stream.included)
                    demuxCt++;
            }
            if (stream.included)
                clipCt++;
        });

        var stepCt = steps.length;
        if (steps.c && !steps.e) {
            // Just to make the progress bar less confusing
            stepCt++;
        }

        // Step one: Demux
        await run("Demuxing...", "plip-demux", [filePath], {
            min: 0, max: 100/stepCt, count: demuxCt
        });
        await detectTracks();

        // Step two: Audio processing
        if (steps.p) {
            await run("Processing audio...", "plip-aproc", [], {min: 100/stepCt, max: 200/stepCt, count: 1});
            await detectTracks();
        }

        // Step three: Editing
        if (steps.e) {
            let tmpBase = dirPath + path.sep + "tmp";
            let tmpMP4 = tmpBase + ".mp4";
            let tmpFLAC = tmpBase + ".flac";
            let tmpJSON = tmpBase + ".json";
            let markFile = filePath.replace(/\.[^\.]*$/, ".mark");

            // But first, we need to mix
            await mix(tmpMP4, {min: 200/stepCt, max: 300/stepCt, count: 4}, {
                title: "Preparing editable file...",
                video: filePath,
                audioState: "proc",
                mixOptions: "-c:v libx264 -c:a aac -crf 23 -threads 0 -preset ultrafast -b:a 128k",
                args: ["-V", "scale=-1:720"]
            });

            // Don't do the waveform conversion if it's already done
            let exists = false;
            try {
                fs.accessSync(tmpJSON);
                exists = true;
            } catch (ex) {}
            if (!exists) {
                // Extract the audio
                await run("Making audio waveform...", formats.ffmpeg, ["-i", tmpMP4, "-map", "0:a", tmpFLAC], {min: 225/stepCt, max: 300/stepCt, count: 3});

                // Convert it to a waveform
                await run("Making audio waveform...", formats.audiowaveform,
                        ["-i", tmpFLAC, "--pixels-per-second", "64", "-b", "8", "-o", tmpJSON],
                        {min: 250/stepCt, max: 300/stepCt, count: 2});
                try {
                    fs.unlinkSync(tmpFLAC);
                } catch (ex) {}
            }

            // Then launch the actual editor
            let config = {
                media: tmpMP4,
                waveform: tmpJSON,
                inMarks: markFile,
                outMarks: markFile
            };
            await new Promise(function(res, rej) {
                let editWindow = new BrowserWindow({
                    width: 1600,
                    height: 900,
                    backgroundColor: "#111",
                    webPreferences: {
                        nodeIntegration: true
                    }
                });

                editWindow.setMenu(null);
                editWindow.maximize();
                editWindow.loadURL(`file://${__dirname}/medit.html?c=` + encodeURIComponent(JSON.stringify(config)));
                editWindow.on("closed", res);
            });
        }

        // Step three: Clipping
        if (steps.c) {
            await run("Clipping...", "plip-clip", [filePath], {min: 300/stepCt, max: 400/stepCt, count: clipCt});
            await detectTracks();
        }

        // Step four: Mixing
        if (steps.m) {
            try {
                fs.unlinkSync(mixPath);
            } catch (ex) {}
            await mix(mixPath, {min: 400/stepCt, max: 500/stepCt, count: 1});
            await detectTracks();
        }

        // Done
        //outputBox.style.display = "none"; // FIXME: This shouldn't just vanish
        var outputTA = gebi("output");
        outputTA.innerText += "\n\nFinished.";
        outputTA.scrollTop = outputTA.scrollHeight;
        outputBoxClose.style.display = "";
        gebi("process").disabled = false;
    }

    // Perform the final mix
    function mix(mixPath, progress, config) {
        let args = [];
        config = config || {};

        // Form our arguments. First, mix options.
        let mo = config.mixOptions || gebi("mixOptions").innerText;
        if (mo !== "") {
            // Options to pass through
            args.push("-o");
            mo = mo.trim().split(/ +/g);
            args = args.concat(mo);

            // Always use -movflags +faststart
            if (/\.mp4$/i.test(mixPath)) {
                args.push("-movflags");
                args.push("+faststart");
            }

            args.push(";");
        }

        // Now any extra config arguments
        if (config.args)
            args = args.concat(config.args);

        // Now the output file
        args.push(mixPath);

        // Then the input video
        let vid = null;
        if (config.video)
            vid = config.video;
        else {
            streams.forEach((stream) => {
                if (stream.type !== "Video" || !stream.included)
                    return;
                if (!vid)
                    vid = dirPath + path.sep + stream.title + "." + formats.vformat;
            });
        }
        if (!vid)
            return; // FIXME
        args.push(vid);

        // Then the input audio
        let asuffix = "." + formats.aformat;
        if (config.audioState === "proc")
            asuffix = "-proc." + formats.aiformat;
        streams.forEach((stream) => {
            if (stream.type !== "Audio" || !stream.included)
                return;
            args.push(dirPath + path.sep + stream.title + asuffix);
        });

        // Then perform the call
        return run(config.title || "Mixing...", "plip-mix", args, progress);
    }

    // Run a command in the background, with output going to outputBox, returning a promise
    function run(label, cmd, args, progress) {
        var outputTA = gebi("output");
        var progressBar = gebi("progress");
        progress = progress || {min: 0, max: 100, count: 1};
        var progressRange = progress.max - progress.min;
        progressBar.value = progress.min;

        return new Promise(function(res, rej) {
            // Full output
            var output = label ? (label+"\n") : "";

            // Plip-only output
            var plipOutput = output;

            // Number of plip lines we've had
            var plipLines = 0;

            // Latest line of output
            var outputLine = "";

            // Number of dots if we're outputting an ellipses ("progress is being made" indicator)
            var dots = 0;

            // We finish when both stdout and stderr have been closed
            var stdoutOpen = true, stderrOpen = true, exitCode = null;

            var p = cp.spawn(cmd, args, {
                cwd: dirPath,
                stdio: ["ignore", "pipe", "pipe"],
            });

            function onData(chunk) {
                chunk = chunk.toString("utf8");

                // Add any complete lines
                var lines = chunk.split("\n");
                while (lines.length > 1) {
                    outputLine += lines.shift();
                    outputLine = processLine(outputLine);
                    if (/^\^PLIP:/.test(outputLine)) {
                        plipOutput += "\n" + outputLine.slice(7);
                        plipLines++;
                        progressBar.value = (plipLines/progress.count) * progressRange + progress.min;
                    }
                    output += "\n" + processLine(outputLine);
                    outputLine = "";
                }

                // Prepare the next line
                outputLine += lines.shift();

                display();

            }

            function display() {
                // Set up our output ellipsis
                var lastLine = processLine(outputLine);
                if (lastLine === "") {
                    if (++dots === 4)
                        dots = 1;
                    lastLine = ("...").slice(0, dots);
                }

                // Output what we have
                outputTA.innerText = plipOutput + "\n" + lastLine;
                outputTA.scrollTop = outputTA.scrollHeight;
            }

            // Update the display every second to show our ellipsis
            var displayInterval = setInterval(display, 1000);

            p.stdout.on("data", onData);
            p.stderr.on("data", onData);

            p.stdout.on("end", function() {
                stdoutOpen = false;
                maybeFinish();
            });

            p.stderr.on("end", function() {
                stderrOpen = false;
                maybeFinish();
            });

            p.on("exit", function(code, signal) {
                if (exitCode !== null) return;
                if (code !== null)
                    exitCode = code;
                else
                    exitCode = signal;
                maybeFinish();
            });

            p.on("error", function(err) {
                if (exitCode !== null) return;
                exitCode = err;
                maybeFinish();
            });

            // Finish if every part is finished
            function maybeFinish() {
                if (stdoutOpen || stderrOpen || exitCode === null)
                    return;

                clearInterval(displayInterval);
                progressBar.value = progress.max;
                res(exitCode);
            }
        });

    }

    // Process a single line. In particular, this deals with \r overwriting
    function processLine(line) {
        line = line.trimEnd().split("\r");
        return line[line.length-1];
    }

    // Delete our intermediate files
    gebi("delete").onclick = function() {
        var sure = gebi("deleteSureBox");
        sure.style.display = "";
        sure.scrollIntoView();
    };

    gebi("deleteSureYes").onclick = async function() {
        gebi("deleteSureBox").style.display = "none";
        spin();

        await detectTracks();

        var base = dirPath + path.sep;

        // Go through every track...
        streams.forEach((stream) => {
            if (stream.type === "Video") {
                // Demux: Delete the track file
                del(base + stream.title + ".track");

                // Clip: Delete the output file (FIXME: multipart)
                del(base + stream.title + "." + formats.vformat);

            } else { // Audio
                // Demux: Delete the raw file
                del(base + stream.title + "-raw." + formats.aiformat);

                // Process: Delete the processed file
                del(base + stream.title + "-proc." + formats.aiformat);

                // Clip: Delete the output file (FIXME: multipart)
                del(base + stream.title + "." + formats.aformat);

            }
        });

        // And delete the editing temporaries
        del(base + "tmp.mp4");
        del(base + "tmp.json");

        // Deletion helper function
        function del(file) {
            try {
                fs.unlinkSync(file);
            } catch (ex) {}
        }

        await detectTracks();

        unspin();
    };

    gebi("deleteSureNo").onclick = function() {
        gebi("deleteSureBox").style.display = "none";
    };

    // We don't really support keyboard controls, but let ctrl+W quit
    document.body.addEventListener("keydown", function(ev) {
        if (ev.keyCode === 87 /* w */ && ev.ctrlKey)
            window.close();
    });

    // Load in our default config
    if (config.file) {
        let file = path.resolve(config.file);
        gebi("inputBox").innerText = file;
        loadFile(file);
    }

})();
