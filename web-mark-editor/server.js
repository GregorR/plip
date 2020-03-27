#!/usr/bin/env node
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

const fs = require("fs");
const http = require("http");
const path = require("path");

const base = path.dirname(module.filename);

if (process.argv.length < 6) {
    console.error("Use: server.js <media file> <waveform file> <in marks> <out marks>");
    process.exit(1);
}

const mediaFile = process.argv[2];
const waveFile = process.argv[3];
const inMarks = process.argv[4];
const outMarks = process.argv[5];

// Set up our HTTP server
const server = http.createServer(request);
server.listen({
    host: "127.0.0.1",
    port: 8085
}, (err) => {
    if (err) {
        console.error("Failed to start HTTP server: " + err);
        process.exit(1);
    }
});

const rangeRE = /=([0-9]+)-([0-9]*)/;

function request(req, resp) {
    var lo, hi, hasRange = false;

    function give(f, absolute) {
        var fn = (absolute?"":(base+path.sep))+f;

        var sbuf = fs.statSync(fn);

        // Our response changes based on whether it asked for a range
        if (hasRange) {
            // Sanitize the range
            if (lo >= sbuf.size)
                lo = sbuf.size - 1;
            if (hi === 0)
                hi = sbuf.size - 1;
            if (hi < lo)
                hi = lo;

            // Set the headers
            resp.setHeader("Content-Range", `bytes ${lo}-${hi}/${sbuf.size}`);
            resp.setHeader("Content-Length", (hi-lo+1)+"");
            if (lo > 0 || hi < sbuf.size - 1)
                resp.writeHead(206);

            fs.createReadStream(fn, {start: lo, end: hi}).pipe(resp);

        } else {
            // Simpler headers
            resp.setHeader("Content-Length", sbuf.size+"");
            fs.createReadStream(fn).pipe(resp);

        }
    }

    var rangeH;
    if ("range" in req.headers && (rangeH = rangeRE.exec(req.headers["range"])) !== null) {
        hasRange = true;
        lo = Number(rangeH[1]);
        hi = Number(rangeH[2]);
    }

    resp.setHeader("Accept-Ranges", "bytes");

    switch (req.url) {
        case "/":
        case "/index.html":
            give("index.html");
            break;

        case "/wme.js":
            give("wme.js");
            break;

        case "/in.mark":
            try {
                fs.accessSync(outMarks);
                give(outMarks, true);
            } catch (ex) {
                give(inMarks, true);
            }
            break;

        case "/out.mark": (function() {
            var data = "";
            req.on("data", (chunk) => {
                data += chunk;
            });
            req.on("end", () => {
                fs.writeFileSync(outMarks, data, "utf8");
            });
            resp.end();
        })(); break;

        case "/media.mp4":
            give(mediaFile, true);
            break;

        case "/wave.json":
            give(waveFile, true);
            break;

        default:
            resp.write("Huh?");
            resp.end();
    }
}
