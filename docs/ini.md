This file documents the aspects of plip's configuration which are editable
through `plip.ini`. This file is divided into each INI group (i.e., `[group]`),
then each setting within that group. Some settings may be refined per track, as
specified in README.md.


# programs

Paths to programs used by plip. The settings `ffmpeg`, `ffprobe`,
`audiowaveform`, and `audacity` default to themselves. The setting `texteditor`
is OS-specific.


# formats

Formats, codecs, etc that plip uses.

## vformat

Format for clipped video. Default `mkv`. May be refined by track, but this will
confuse the GUI.

## vcodec

Codec for clipped video. Default `libx264`. May be refined by track.

## vcrf

x264 CRF for clipped video. Default `16`. Set this to an empty string to use
bitrate (next setting) or default (no setting) encoding quality. May be refined
by track.

## vbr

Video bitrate for clipped video. Fixed-quality encoding is usually a better
option. Default unset. May be refined by track.

## vflags

ffmpeg flags to use for clipped video. These flags replace vcodec, vcrf, and
vbr above. Default unset. May be refined by track.

## aiformat

Format for intermediate audio files. Default `flac`.

## aicodec

Codec for intermediate audio files. Default `flac`.

## aformat

Format for clipped audio files. Default `wav`. May be refined by track, but
this will confuse the GUI.

## acodec

Codec for clipped audio files. Default `pcm_s16le`. May be refined by track.

## abr

Audio bitrate for clipped audio. Default unset. May be refined by track.


# tracks

Specifies which tracks are to be included or excluded.

## include

Set to `y` or `n` to include or exclude a track, respectively. Default `y`.
Should be refined by track.


# steps

Defines the steps to be performed in processing.

## noiser

Noise reduction engine to use, or blank for no noise reduction. May be
`noiserepellent`, `speex`, or blank. Default `noiserepellent`. May be defined
by track.

## aproc

Set to the number of processing steps that should be performed. Default `2`.
May be refined by track.

## videobypass

If set, video processing will be bypassed by the specified script. Default
unset. May be refined by track.


# filters

Filters to use during audio processing (for audio) or clipping (for video). The
number of steps is defined in the above section; this section specifies what
the steps actually are.

## aproc1

The name of the first filtering step to use. Similarly for `aproc2`, etc.
Default `aproc1=compress`, `aproc2=level`. May be refined by track.

## alevel1

Target volume level, in decibels, for this step. If this is set to something
other than `0`, plip will perform audio level analysis before the actual
filter, so that the filter may be refined based on the needed gain to reach
this target volume. Default `alevel1=-18`, `alevel2=-18`. May be refined by
track.

## (filters)

Actual filters are defined by the names given by `aproc1`, `aproc2`, etc. These
are filters in ffmpeg's format, with a `level` variable based on `alevel1`
above. Default `compress=acompressor=level_in=$(level)dB`,
`level=alimiter=level_in=$(level)dB:level=0`, etc. Other filters are available;
see the output of `plip-defconfig`. May be refined by track, but shouldn't be;
refine `aproc1` etc instead.

## video

Video filters to perform, in ffmpeg's format. Default `null`. May be refined by
track.

## ffclip

What to do with audio during fast-forwarded segments. May be `discard`, `keep`,
or `fast`. Default `discard`. May be refined by track.
