/*
 * Copyright (c) 2020 Gregor Richards
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
    const fs = require("fs");
    const path = require("path");

    const url = new URL(window.location);
    const urlParams = new URLSearchParams(url.search);
    const config = JSON.parse(urlParams.get("c"));

    const geid = document.getElementById.bind(document);
    const ce = document.createElement.bind(document);

    // "Configurable" globals
    let fflen = 8;
    let minffspeed = 4;

    // Our display boxes
    const speedBox = geid("speed");
    const timeBox = geid("totalTime");
    const media = geid("media");
    const marksBox = geid("marks");
    const jumpBox = geid("jump");
    const jumpToBox = geid("jumpTo");
    let curPosBox = null;

    // Our current marks
    let marks = [];

    // Our current position in time, and surrounding marks
    let curPos = 0;
    let curPrevMark = null, curIndex = 0, curNextMark = null;
    let totalTime = 0;

    // Some common display colors
    const grey = "#111";
    const red = "#200";
    const green = "#020";
    const blue = "#002";
    const yellow = "#220";

    // Load our media
    media.src = path.resolve(config.media);

    // Create the wavesurfer display
    let wavesurfer = null;
    geid("waveform").style.backgroundColor = grey;

    // Load our waveform data
    let waveform = JSON.parse(fs.readFileSync(config.waveform, "utf8"));
    let waveformDiv = 1 << (waveform.bits-1);

    // We assume we've got too much wave for wavesurfer, so divide it into 10-minute chunks
    let wavesurferOffset = -1, wavesurferDuration = 600;
    let wavesurferZoom = 64;

    // A fake media element representing part of the real one, for wavesurfer's sake
    function FakeMediaElement(real, offset, duration) {
        this.real = real;
        this.src = real.src;
        this.duration = duration;
        this.paused = true;
        this.playHandler = null;

        Object.defineProperty(this, "currentTime", {
            get: function() {
                return real.currentTime - offset;
            },
            set: function(to) {
                if (to !== 0)
                    real.currentTime = to + offset;
            }
        });
    }

    FakeMediaElement.prototype.addEventListener = function(type, listener, options) {
        listener = listener.bind(this);
        if (type === "play") this.playHandler = listener;
        this.real.addEventListener(type, listener);
    }

    FakeMediaElement.prototype.removeEventListener = function(type, listener, options) {
        // FIXME
        return;
    }

    FakeMediaElement.prototype.pause = function() {
        this.paused = true;
    }
    
    FakeMediaElement.prototype.play = function() {
        this.paused = false;
        if (this.playHandler)
            this.playHandler();
    }

    // Update wavesufer to see only a ten-minute subsection
    function updateWavesurfer() {
        // Figure out if we should update at all
        if (!waveform) return false;
        if (Number.isNaN(media.duration)) return false;
        var newWavesurferOffset = ~~(curPos / 100) * 100 - 100;
        if (newWavesurferOffset < 0) newWavesurferOffset = 0;
        if (wavesurferOffset === newWavesurferOffset) return false;
        wavesurferOffset = newWavesurferOffset;

        // Destroy and replace the old one
        if (wavesurfer)
            wavesurfer.destroy();
        wavesurfer = WaveSurfer.create({
            container: "#waveform",
            backend: "MediaElement",
            mediaControls: true,
            mediaType: "video",
            removeMediaElementOnDestroy: false,
            hideScrollbar: true,
            plugins: [
                WaveSurfer.regions.create({})
            ]
        });

        // And load in a chunk of data
        var fakey = new FakeMediaElement(media, wavesurferOffset, wavesurferDuration);
        var toPix = waveform.sample_rate / waveform.samples_per_pixel * 2;
        var base = wavesurferOffset * toPix;
        var len = wavesurferDuration * toPix;
        var end = base + len;
        var waveformPart = waveform.data.slice(base, end);
        waveformPart = waveformPart.map(function(x) {
            return x / waveformDiv;
        });
        while (waveformPart.length < len)
            waveformPart.push(0);
        wavesurfer.loadMediaElement(fakey, waveformPart);
        wavesurfer.zoom(wavesurferZoom);

        wavesurfer.on("ready", wavesurfer.play.bind(wavesurfer));

        return true;
    }

    media.addEventListener("canplaythrough", updateWavesurfer);
    media.addEventListener("durationchange", updateWavesurfer);

    // Utility function to translate seconds into h:m:s
    function prettyTime(s, sh) {
        var h = ~~(s / 3600);
        s -= h * 3600;
        var m = ~~(s / 60);
        s -= m * 60;
        if (!sh || h) {
            if (m < 10) m = "0"+m;
        }
        if (!sh || h || m) {
            if (s < 10)
                s = "0"+s.toFixed(2);
            else
                s = s.toFixed(2);
        } else
            s = s.toFixed(2);

        if (sh) {
            var ret = "";
            if (h)
                ret += h + ":";
            if (h || m)
                ret += m + ":";
            ret += s;
            return ret;
        } else {
            return h+":"+m+":"+s;
        }
    }

    // Read in the original marks
    (function() {
        let marksRaw = [];
        try {
            marksRaw = fs.readFileSync(config.inMarks, "utf8").split("\n");
        } catch (ex) {} // input file is allowed to not exist
        marksRaw.forEach((marksLine) => {
            if (marksLine.length)
                marks.push({"e": marksLine[0], "t": Number(marksLine.slice(1))});
        });
        updateMarks();
    })();

    // Validate that our marks make sense
    function validateMarks(full) {
        marks.sort(function (a, b) {
            return a.t - b.t;
        });

        if (!full) return;

        var mode = "o";
        var i;
        var mark;

        function del() {
            marks.splice(i, 1);
            i--;
        }

        function ins(e, rep) {
            rep = rep?rep:0;
            var newMark = {e: e, t: mark.t};
            marks.splice(i, rep, newMark);
            i--;
        }

        for (i = 0; i < marks.length; i++) {
            mark = marks[i];
            if (mark.e === "m") continue;
            switch (mode + mark.e) {
                case "ii": del(); continue;
                case "in": del(); continue;
                case "ir": ins("o"); continue;
                case "of": ins("i"); continue;
                case "on": del(); continue;
                case "oo": del(); continue;
                case "fi": ins("n", 1); continue;
                case "fo": ins("n"); continue;
                case "ff": del(); continue;
                case "fr": ins("o"); ins("n"); continue;
                case "ni": del(); continue;
                case "nn": del(); continue;
                case "nr": ins("o"); continue;
            }
            if (mark.e[0] === "x") {
                del();
                continue;
            }
            mode = (mark.e==="n") ? "i" : mark.e;
        }
    }

    // Update marks based on the current marks array
    function updateMarks(send) {
        // Always validate the order first
        validateMarks();

        // Keep wavesurfer happy
        updateWavesurfer();

        // Have we marked our own location yet?
        var curMarked = false;

        // What color should the web page background be?
        var bgC = red;

        // Calculate our total time as we go
        totalTime = 0;
        var lastReset = 0;
        var mLastIn = null, mLastFast = null, mLastNorm = null;

        // Start from nothing
        marksBox.innerHTML = "";

        // The marker for our current position
        function addCurPos() {
            curPosBox = ce("div");
            curPosBox.innerText = "• " + prettyTime(curPos);
            curPosBox.style.fontSize = "2em";
            marksBox.appendChild(curPosBox);
            curMarked = true;
        }

        // Simple mark color generator
        function markToColor(mark) {
            var c = grey;
            switch (mark.e) {
                case "i": c = green; break;
                case "o": c = red; break;
                case "f": c = yellow; break;
                case "n": c = green; break;
                case "m": c = blue; break;
            }
            return c;
        }

        // We learn our previous and next mark while updating
        curPrevMark = curNextMark = null;

        // Start with no regions
        wavesurfer && wavesurfer.clearRegions();

        marks.forEach(function (mark, idx) {
            if (!curMarked) {
                if (mark.t > curPos) {
                    // Mark ourself first
                    addCurPos();
                    curIndex = idx;
                    curNextMark = mark;
                } else if (mark.e !== "m" && mark.e[0] !== "x") {
                    // Still looking for the previous mark
                    curPrevMark = mark;
                }
            }

            // Add this mark
            var l = ce("div");
            l.innerText = mark.e + " " + prettyTime(mark.t);
            l.style.backgroundColor = markToColor(mark);
            l.onclick = function() {
                media.currentTime = mark.t;
                updateMarks();
            }
            marksBox.appendChild(l);

            // Add a region
            function addRegion(from, to, color) {
                if (!wavesurfer) return;
                if (from === null) return;
                var fromT = from.t;
                var toT = to ? to.t : (fromT+0.125);

                if (toT < wavesurferOffset)
                    return;
                if (fromT > wavesurferOffset + wavesurferDuration)
                    return;
                fromT -= wavesurferOffset;
                toT -= wavesurferOffset;
                if (fromT < 0) fromT = 0;
                if (toT > wavesurferDuration) toT = wavesurferDuration;

                var r = wavesurfer.addRegion({
                    start: fromT,
                    end: toT,
                    color: color
                });
                r.startMark = from;
                r.endMark = to;
                r.drag = r.resize = false;
            }

            // Turn the marks into regions and total time
            switch (mark.e) {
                case "i":
                    mLastIn = mLastNorm = mark;
                    break;

                case "f":
                    mLastFast = mark;
                    if (curPos >= lastReset && mLastNorm)
                        totalTime += mark.t - mLastNorm.t;
                    break;

                case "o":
                    addRegion(mLastIn, mark, "rgba(0,255,0,0.1)");
                    if (curPos >= lastReset && mLastNorm)
                        totalTime += mark.t - mLastNorm.t;
                    mLastIn = mLastFast = mLastNorm = null;
                    break;

                case "n":
                    addRegion(mLastFast, mark, "rgba(255,128,0,0.2)");
                    if (curPos >= lastReset && mLastFast) {
                        var fftime = mark.t - mLastFast.t;
                        if (fftime > (fflen*minffspeed))
                            totalTime += fflen;
                        else
                            totalTime += fftime / minffspeed;
                        mLastNorm = mark;
                        l.innerText += " (" + prettyTime(mark.t - mLastFast.t, true) + ")";
                    }
                    mLastFast = null;
                    break;

                case "m":
                    addRegion(mark, null, "rgba(0,0,255,0.5)");
                    break;

                case "r":
                    mLastIn = mLastFast = mLastNorm = null;
                    if (curPos >= mark.t) {
                        lastReset = mark.t;
                        totalTime = 0;
                    }
                    break;
            }

            mark.vt = totalTime;
        });

        if (!curMarked) {
            addCurPos();
            curIndex = marks.length;
            curNextMark = null;
        }

        // Make sure the current one is on screen
        marksBox.style.top = -(curPosBox.offsetTop - (window.innerHeight/2)) + "px";

        // Update our background to correspond
        if (curPrevMark)
            bgC = markToColor(curPrevMark);
        document.body.style.backgroundColor = bgC;

        // And our time indicator
        updateTotalTime();

        // And finally, send them to the server
        if (marks.length && send) {
            sendMarks();
        }
    }

    // Update the time indicator
    function updateTotalTime() {
        var curTime = curPos;
        if (curPrevMark) {
            switch (curPrevMark.e) {
                case "i":
                case "n":
                    curTime -= curPrevMark.t - curPrevMark.vt;
                    break;

                default:
                    curTime = curPrevMark.vt;
            }
        }
        timeBox.innerText = prettyTime(curTime) + " / " + prettyTime(totalTime);
    }

    // Update a single mark which has had a time change
    function updateMark(mark) {
        var origIdx = -1, newIdx = -1;

        // Find where to remove and insert it
        marks.forEach((otherMark, idx) => {
            if (mark === otherMark)
                origIdx = idx;
            if (newIdx < 0 && otherMark.t > mark.t)
                newIdx = idx;
        });
        if (newIdx === -1) newIdx = marks.length;

        // And do so
        if (origIdx < 0) {
            marks.splice(newIdx, 0, mark);

        } else if (origIdx < newIdx) {
            marks.splice(newIdx, 0, mark);
            marks.splice(origIdx, 1);

        } else if (origIdx > newIdx) {
            marks.splice(origIdx, 1);
            marks.splice(newIdx, 0, mark);

        }
    }

    var sendAgain = false;

    // Save our marks
    function sendMarks() {
        var marksTxt = "";
        marks.forEach((mark) => {
            if (mark.e[0] === "x") return;
            marksTxt += mark.e + mark.t + "\n";
        });
        fs.writeFileSync(config.outMarks, marksTxt);
    }

    // Quick-update the marks, doing a full update only if we've passed a mark
    function quickUpdateMarks() {
        if (!curPosBox) {
            updateMarks();
            return;
        }

        if (curNextMark && curPos >= curNextMark.t) {
            updateMarks();
            return;
        }

        if (curPrevMark && curPos < curPrevMark.t) {
            updateMarks();
            return;
        }

        if (updateWavesurfer()) {
            updateMarks();
            return;
        }

        curPosBox.innerText = "• " + prettyTime(curPos);
        updateTotalTime();
    }

    // Follow the video box
    var vidInterval = null;
    var vidClicks = 0;
    function videoFollow() {
        if (++vidClicks >= 10) {
            clearInterval(vidInterval);
            vidInterval = null;
        }

        curPos = media.currentTime;
        quickUpdateMarks();

        if (wavesurfer) {
            if (wavesurfer.isPlaying())
                wavesurfer.pause();
            wavesurfer.play();
        }
    }

    media.addEventListener("timeupdate", function() {
        if (!vidInterval)
            vidInterval = setInterval(videoFollow, 100);
        vidClicks = 0;
    });

    // The subset of marks that are directly jumpable
    var jumps = [];

    // Compute the jumps
    function calculateJumps() {
        jumps = marks.filter(function(mark) {
            return (mark.e === "i" || mark.e === "n");
        });
        geid("jumpLimit").innerText = jumps.length;
    }

    // Insert a mark at the current location, or insert a complete mark object at the correct location
    function insertMark(e, ender, ignore) {
        if (ender)
            insertEnder(ender, ignore);
        var mark = {e: e, t: curPos};
        marks.splice(curIndex, 0, mark);
    }

    // Get the next actually interesting mark, skipping the ignore list
    function nextMark(ignore) {
        if (!ignore) ignore = {};
        for (var i = curIndex; i < marks.length; i++) {
            var mark = marks[i];
            if (mark.e === "m" || ignore[mark.e]) continue;
            return {i: i, m: mark};
        }
        return {i: marks.length, m: null};
    }

    // Eliminate the next mark of the given event, only skipping marks in the ignore list
    function eliminateMark(ev, ignore, hard) {
        var next = nextMark(ignore);
        if (next.m && next.m.e === ev) {
            /* The purpose of keeping this mark is so that if we want to back
             * out, our mark has something to abut to. That's pointless if the
             * next mark is at the same time. This avoids clutter particularly
             * when we created the other mark in the first place. */
            var follow = marks[next.i+1];
            if (follow && follow.t === next.m.t)
                marks.splice(next.i, 1);
            else
                next.m.e = "x" + ev;
        }
    }

    // Insert an appropriate ender before the next mark
    function insertEnder(ev, ignore) {
        var next = nextMark(ignore);
        if (next.m) {
            var mark = {e: ev, t: next.m.t};
            if (next.m.e === "x" + ev)
                marks.splice(next.i, 1, mark);
            else
                marks.splice(next.i, 0, mark);
        } else {
            var mark = {e: ev, t: media.duration};
            marks.splice(marks.length, 0, mark);
        }
    }

    var fastForwarding = {"f": true, "n": true, "xn": true};

    // Perform a cut (in or out)
    function performCut() {
        if (curPrevMark && (curPrevMark.e === "i" || curPrevMark.e === "n")) {
            // We're cutting *out*
            eliminateMark("o", fastForwarding);
            insertMark("o");

        } else if (curPrevMark && curPrevMark.e === "f") {
            // We're stopping a fast-forward
            eliminateMark("n", {});
            insertMark("n");

        } else {
            // We're cutting *in*
            insertMark("i", "o", fastForwarding);

        }
        updateMarks(true);
    }

    // Perform a fast forward or reset
    function performFF() {
        if (!curPrevMark) return;

        switch (curPrevMark.e) {
            case "i":
            case "n":
                insertMark("f", "n", {});
                break;

            case "f":
                eliminateMark("n", {});
                insertMark("n");
                break;

            case "o":
                insertMark("r");
                break;
        }

        updateMarks(true);
    }

    // Delete a mark
    function performDelete(hard) {
        var markToDelete = marks[curIndex-1];
        if (!markToDelete) return;

        switch (markToDelete.e) {
            case "i": eliminateMark("o", fastForwarding); break;
            case "f": eliminateMark("n"); break;
            case "o": insertEnder("o", fastForwarding); break;
            case "n": insertEnder("n"); break;
        }

        marks.splice(curIndex-1, 1);
        updateMarks(true);
    }

    // "Auto-scrubbing" just scrubs some fixed amount every fixed amount of time
    var autoScrubBy = 0;
    var autoScrubInterval = null;
    var autoScrubResume = false;
    function autoScrubStep() {
        media.currentTime += autoScrubBy;
    }

    function autoScrub(by) {
        if (!autoScrubInterval) {
            autoScrubBy = by;
            autoScrubResume = !media.paused;
            media.pause();
            autoScrubInterval = setInterval(autoScrubStep, 100);
            autoScrubStep();
        }
    }

    // Finally, our controls
    function controls(ev) {
        switch (ev.keyCode) {
            case 32: // Space
            case 75: // K
                if (media.paused)
                    media.play();
                else
                    media.pause();
                return false;

            case 19: // Pause
            case 70: // F
                performCut();
                break;

            case 145: // Scroll lock
            case 69: // E
                insertMark("m");
                updateMarks(true);
                document.body.backgroundColor = blue;
                setTimeout(updateMarks, 250);
                break;

            case 42: // Printscreen
            case 83: // S
                performFF();
                break;

            case 46: // Delete
            case 68: // D
                performDelete();
                break;

            case 86: // V (validate)
                validateMarks(true);
                updateMarks(true);
                break;

            case 37: // Left
            case 74: // J
                if (ev.shiftKey)
                    autoScrub(-5);
                else
                    autoScrub(-0.1);
                break;

            case 39: // Right
            case 76: // L
                if (ev.shiftKey)
                    autoScrub(5);
                else
                    autoScrub(0.1);
                break;

            case 72: // H
                if (ev.shiftKey)
                    autoScrub(-30);
                else
                    autoScrub(-1);
                break;

            case 59: // ;
            case 186: // ;
                if (ev.shiftKey)
                    autoScrub(30);
                else
                    autoScrub(1);
                break;

            case 38: // Up
            case 73: // I
                if (curPrevMark)
                    media.currentTime = curPrevMark.t - 0.1;
                else
                    media.currentTime = 0;
                updateMarks();
                break;

            case 40: // Down
            case 188: // ,
                if (curNextMark)
                    media.currentTime = curNextMark.t;
                else
                    media.currentTime = media.duration;
                updateMarks();
                break;

            case 80: // P
                if (jumpBox.style.display === "none") {
                    calculateJumps();
                    jumpBox.style.display = "block";
                    jumpToBox.focus();
                    jumpToBox.select();
                } else {
                    jumpBox.style.display = "none";
                }
                break;

            case 48: // 0
            case 49: // ...
            case 50:
            case 51:
            case 52:
            case 53:
            case 54:
            case 55:
            case 56:
            case 57: // 9
                media.currentTime = (media.duration / 10) * (ev.keyCode-48);
                updateMarks();
                break;

            case 61: // =/+
            case 107: // Numpad +
            case 79: // O
                media.playbackRate++;
                speed.innerText = media.playbackRate + "x";
                break;

            case 173: // -/_
            case 109: // Numpad -
            case 85: // U
                media.playbackRate--;
                speed.innerText = media.playbackRate + "x";
                break;

            case 33: // PgUp
                wavesurferZoom /= 2;
                if (wavesurferZoom < 8)
                    wavesurferZoom = 8;
                wavesurferOffset = -1;
                updateMarks();
                break;

            case 34: // PgDown
                wavesurferZoom *= 2;
                if (wavesurferZoom > 512)
                    wavesurferZoom = 512;
                wavesurferOffset = -1;
                updateMarks();
                break;

            case 191: // ? (help)
                var hlp = geid("help");
                if (hlp.style.display === "none")
                    hlp.style.display = "block";
                else
                    hlp.style.display = "none";
                break;

            case 87: // w
                if (ev.ctrlKey)
                    window.close();
                
            default:
                // Unrecognized
                //geid("debug").innerText = ev.keyCode;
                return true;
        }

        ev.stopPropagation();
        ev.preventDefault();
        return false;
    }

    // Controls to turn off auto-scrubbing
    function controlsUp(ev) {
        switch (ev.keyCode) {
            case 37: // Left
            case 74: // J
            case 39: // Right
            case 76: // L
            case 72: // H
            case 59: // ;
            case 186: // ;
                if (autoScrubInterval) {
                    clearInterval(autoScrubInterval);
                    autoScrubInterval = null;
                    if (autoScrubResume)
                        media.play();
                }
                break;
        }
    }

    document.body.addEventListener("keydown", controls);
    document.body.addEventListener("keyup", controlsUp);
    media.addEventListener("keydown", controls);
    media.addEventListener("keyup", controlsUp);

    // The jump box has its own controls
    jumpToBox.addEventListener("keydown", function(ev) {
        ev.stopPropagation();

        if (ev.keyCode !== 13) return true;

        // We've chosen where to jump to!
        var jumpTo = jumps[Number(this.value)-1];
        if (jumpTo) {
            media.currentTime = jumpTo.t;
            updateMarks();
        }

        if (jumpTo || this.value === "")
            jumpBox.style.display = "none";

        return false;
    });

    // And the help button opens help
    geid("helpB").onclick = function() {
        geid("help").style.display = "block";
    };

    window.addEventListener("resize", updateMarks);
})();
