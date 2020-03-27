# plip

plip is a batch and linear video editor. Yes, you read that right: It's a
linear editor, not a nonlinear video editor, and proud of it!

The basic idea of plip is to simplify or automate the more tedious tasks of
video editing, while leaving the more sophisticated tasks to standard nonlinear
video editors. A linear video editor is capable of including or excluding
segments, but not reordering them or repeating them. For many video editing
tasks, this is a large part or even all editing required, and the video editing
process can be sped up by having a simple tool perform these tasks rather than
relying on a nonlinear video editor for everything.

The workflow for plip is as follows:

1. Optionally, during recording, using a monitor such as the included monitor
   for OBS, you create a marks file which records what sections of the recorded
   video you want to keep, and what sections you want to discard.

2. plip demuxes and performs automated audio processing, such as dynamic range
   compression, on the audio.

3. Optionally, you use plip's integrated marks editor to select sections of the
   video you want to include or exclude. This is a simple editor meant for fast
   and easy editing, but intentionally is not capable of the power of a
   nonlinear video editor.

4. plip clips your video and audio based on the marks created by the monitor
   and/or integrated editor.

5. You use either a nonlinear video editor for final edits, or plip's integrated
   mixer to finalize your video.

plip is based heavily (in essence, entirely) on ffmpeg ( https://ffmpeg.org ),
which does all the heavy lifting. plip itself just organizes the tasks.


## GUI

plip's GUI is optional, but of course, for users accustomed to a graphical user
interface, it is a good option. Run plip's GUI with the plip-gui (or
plip-gui.exe) command. Simply select a file, select an output directory if
you'd like (defaults to the same directory as the file), and click "process" to
perform plip's usual processing steps. You can enable or disable steps, as well
as include or exclude files, from the GUI.

Most of plip's behavior, however, is controlled by its configuration files. If
you want to customize its behavior, you will need to learn to edit them.


## Monitor

To generate marks for inclusion/exclusion of video segments while recording,
install plip's OBS monitor to OBS's plugins directory, and include `plip-util`
in every scene you intend to use. It may be hidden, but it must be included to
work.

Configure buttons to start, stop, and fast-forward marks in OBS's hotkey
configuration. While recording, the monitor will show which mode you're in
through its color, and how much included time has elapsed. It starts in
excluded (stopped) mode, so make sure you start it after starting recording!


## Configuration

plip's configuration file is named `plip.ini`, and largely follows the
venerable INI file standard. If you are familiar with INI files, you should
understand the basics immediately. If not, INI files look like this:

```
[group]
setting=value
```

The particular groups and settings available are described in docs/ini.md .

When you use the standard `plip.ini` and not a custom configuration file, plip
is designed to refine configuration based on the directory. For instance, if
you are processing a project in `/home/plip/video/135`, plip will load any
`plip.ini` files found in `/`, `/home`, `/home/plip`, and `/home/plip/video`
before `/home/plip/video/135/plip.ini` itself. In this way, you can set a
default configuration for all of your video projects, and refine it for
specific projects or subprojects.

In addition to standard settings, plip has a couple tricks up its sleeves. Many
settings can be refined for specific tracks. To do so, specify a *regular
expression* of the track name before the setting. For instance, to exclude a
track named `voice2`:

```
[tracks]
/^voice2$/include=n
```

If you're not familiar with regular expressions, just follow the pattern above.
If you are, you can hopefully see how powerful this refinement can be.

Finally, since plip expects nested INI files which refine each other, you can
append to an existing configuration using `+=`:

```
[filters]
video+=,crop=1280:720:0:0
```

Or, prepend to one using `<+=`:

```
[filters]
video<+=fps=30,
```


## Command-line usage

plip is intended to be fully usable from the command line (and indeed, the
author exclusively uses it in this way). plip's steps are demuxing, audio
processing, editing, clipping, and mixing. Demuxing is performed by
`plip-demux`, audio processing by `plip-aproc`, clipping by `plip-clip`, and
mixing by `plip-mix`. See the `--help` output from any of these tools for
usage. As mark editing is a fundamentally visual task, it is only supported
through the GUI.
