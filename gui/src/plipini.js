const fs = require("fs");

const headerRE = /^\s*\[([^\]]*)\]\s*$/;
const entryRE = /^\s*(\/(([^\/\\]|\\.)*)\/|)\s*([^\/<\+=]*)\s*((<?\+)?=)\s*(.*)$/;
const nextableRE = /^(.*)\\\r?$/;
const emptyRE = RegExp("");

function getLine(lines) {
    let rawLine = lines.shift();
    let line = rawLine;
    let res;
    while ((res = nextableRE.exec(line)) !== null) {
        let nextLine = lines.shift();
        line = res[1];
        line += nextLine;
        rawLine += "\n" + nextLine;
    }
    return {rawLine, line};
}

// INI files as a whole
function IniFile(parent, file) {
    this.parent = parent;
    this.file = file;
    this.saving = false;
    this.savingCb = null;

    let fileContent = "";
    try {
        fileContent = fs.readFileSync(file, "utf8");
    } catch (ex) {}
    let rawLines = fileContent.split("\n");
    let head = this.head = new IniLine(this);
    let tail = this.tail = new IniLine(this);
    head.next = tail;
    tail.prev = head;

    // Each group keeps its own list of lines
    let group = new IniGroup(this, "_");
    let groups = this.groups = {_: group};

    // Now read it all in
    while (rawLines.length) {
        let lineParts = getLine(rawLines);
        let rawLine = lineParts.rawLine;
        let line = lineParts.line;
        let il;
        let res;

        if (rawLine === "" && rawLines.length === 0)
            break;

        if ((res = headerRE.exec(line)) !== null) {
            // It's a group header
            if (res[1] in groups) {
                group = groups[res[1]];
            } else {
                group = groups[res[1]] = new IniGroup(this, res[1]);
            }

            il = new IniLine(this, rawLine, res[1]);

        } else if ((res = entryRE.exec(line)) !== null) {
            // It's an entry
            il = new IniLine(this, rawLine, (res[1].length?res[2]:null), res[4], res[5], res[7]);

        } else {
            // Unknown/other
            il = new IniLine(this, rawLine);

        }

        group.push(il);
        this.push(il);
    }
}

// Given a simple array of IniLines, get the actual resulting value
IniFile.getValue = function(lines) {
    let value = "";
    lines.forEach((line) => {
        if (line.type === IniLine.SETTING) {
            if (line.op === "<+=") {
                value = line.value + value;
            } else if (line.op === "+=") {
                value += line.value;
            } else {
                value = line.value;
            }
        }
    });
    return value;
}

// Push a line into this file
IniFile.prototype.push = function(line) {
    line.next = this.tail;
    line.prev = this.tail.prev;
    line.prev.next = line;
    line.next.prev = line;
}

// Get all lines matching the given conditions
IniFile.prototype.get = function(context, groupName, name) {
    let lines = this.parent?this.parent.get(context, groupName, name):[];
    let group = this.groups[groupName];
    if (!group)
        return lines;

    for (let line = group.head.gnext; line.gnext; line = line.gnext) {
        if (line.type !== IniLine.SETTING) continue;
        if (line.name !== name) continue;
        if (!line.conditionRE.test(context)) continue;
        lines.push(line);
    }

    return lines;
}

// Get the actual generated value for a given setting (just get+static getValue)
IniFile.prototype.getValue = function(context, groupName, name) {
    return IniFile.getValue(this.get(context, groupName, name));
}

// Change the INI file such that the given setting becomes true (FIXME: badly generated RE)
IniFile.prototype.set = function(context, groupName, name, value, replace) {
    let group = this.groups[groupName];
    if (!group) {
        // Need to make the group first
        group = new IniGroup(this, groupName);
        this.groups[groupName] = group;

        // For clarity, add a blank line before the group line (not part of any group)
        let line = new IniLine(this, "");
        this.push(line);

        // Now make the group header
        line = new IniLine(this, `[${groupName}]`, groupName);
        group.push(line);
        this.push(line);
    }

    // Make the line itself
    let line = new IniLine(this, (context?"/^"+context+"$/":"")+`${name}=${value}`, "^"+context+"$", name, "=", value);

    // Find the last non-comment line (to insert it after)
    let goodLine;
    if (replace) {
        goodLine = replace;
    } else {
        goodLine = group.tail.gprev;
        while (goodLine.type === IniLine.UNKNOWN)
            goodLine = goodLine.gprev;
    }

    // Now add to the group
    goodLine.insertAfter(line);

    // And if we were replacing, do so
    if (replace) replace.remove();
}

// Set this value, semi-cleverly replacing an existing value if it's specific
IniFile.prototype.update = function(context, groupName, name, value) {
    let replace = null;
    let maybe = this.get(context, groupName, name);

    maybe.forEach((line) => {
        if (context) {
            if (line.condition && line.condition === "^"+context+"$")
                replace = line;
        } else {
            if (!line.condition)
                replace = line;
        }
    });

    this.set(context, groupName, name, value, replace);
}

// Re-serialize
IniFile.prototype.serialize = function() {
    var ret = "";
    for (let line = this.head.next; line.next; line = line.next)
        ret += line.line + "\n";
    return ret;
}

// Save this INI file asynchronously, making sure not to step on our own feet
IniFile.prototype.save = function(then) {
    if (this.saving) {
        // Wait until they're done
        let prevSavingCb = this.savingCb;
        this.savingCb = (err) => {
            prevSavingCb(err);
            this.save(then);
        }
        return;
    }

    // Start our own save
    this.saving = true;
    this.savingCb = then;
    let ser = this.serialize();
    fs.writeFile(this.file, ser, (err) => {
        this.saving = false;
        this.savingCb(err);
    });
}

// INI groups within an INI file
function IniGroup(parent, name) {
    this.name = name;
    this.parent = parent;

    let head = this.head = new IniLine(parent);
    let tail = this.tail = new IniLine(parent);
    head.gnext = tail;
    tail.gprev = head;
}

IniGroup.prototype.push = function(line) {
    line.gnext = this.tail;
    line.gprev = this.tail.gprev;
    line.gprev.gnext = line;
    line.gnext.gprev = line;
}

// An individual INI line, of which there are many forms
function IniLine(parent, line, meta, name, op, value) {
    this.parent = parent;
    this.line = (typeof line==="string")?line:null;
    this.next = this.prev = this.gnext = this.gprev = null;

    switch (arguments.length) {
        case 1:
            this.type = IniLine.NIL;
            break;

        case 3:
            this.type = IniLine.HEADER;
            this.name = meta;
            break;

        case 6:
            this.type = IniLine.SETTING;
            this.condition = meta?meta:null;
            this.conditionRE = meta?RegExp(meta):emptyRE;
            this.name = name;
            this.op = op;
            this.value = value;
            break;

        default:
            this.type = IniLine.UNKNOWN;
            break;
    }
}

IniLine.NIL         = 0;
IniLine.UNKNOWN     = 1;
IniLine.HEADER      = 2;
IniLine.SETTING     = 3;

// Remove this line entirely
IniLine.prototype.remove = function() {
    if (this.next && this.prev) {
        this.next.prev = this.prev;
        this.prev.next = this.next;
        this.next = this.prev = null;
    }

    if (this.gnext && this.gprev) {
        this.gnext.gprev = this.gprev;
        this.gprev.gnext = this.gnext;
        this.gnext = this.gprev = null;
    }
}

// Insert a line after this one in both orderings
IniLine.prototype.insertAfter = function(line) {
    line.prev = this;
    line.next = this.next;
    this.next.prev = line;
    this.next = line;

    line.gprev = this;
    line.gnext = this.gnext;
    this.gnext.gprev = line;
    this.gnext = line;
}

module.exports = {IniFile};
