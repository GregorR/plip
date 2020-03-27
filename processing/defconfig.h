static const char defConfig[] =

// Can be swapped out for specific versions/paths
"[programs]\n"
"ffmpeg=ffmpeg\n"
"ffprobe=ffprobe\n"
"audiowaveform=audiowaveform\n"

// Programs that vary per system
#ifdef _WIN32
"texteditor=notepad\n"
"audacity=C:\\\\Program Files (x86)\\\\Audacity\\\\Audacity.exe\n"
#else
"texteditor=gnome-text-editor\n"
"audacity=audacity\n"
#endif

// ffmpeg filters and their configuration
"\n[filters]\n"

// aproc filter 1 is compression
"aproc1=compress\n"
"alevel1=-18\n"
"compress=acompressor=level_in=$(level)dB\n"

// aproc filter 2 is leveling
"aproc2=level\n"
"alevel2=-18\n"
"level=alimiter=level_in=$(level)dB:level=0\n"

// an extra aproc filter for those who want it, inverse gate
"\nigate=alimiter=level_in=$(level)dB:level=0,aformat=channel_layouts=stereo [aud]; sine [pure]; amovie=$(dep1) [voice]; \\\n"
    "[pure][voice] sidechaingate=level_in=8:ratio=9000:attack=10:release=1000,atrim=0.2,agate=threshold=0.5:ratio=9000 [gate]; \\\n"
    "[aud][gate]   sidechaincompress=level_in=0.5:threshold=-20dB:ratio=20:attack=1000:release=1000,alimiter=level_in=-8dB:level=0\n\n"

// the resampling filter used during demuxing
"resample=aresample=flags=res:min_comp=0.001:min_hard_comp=0.1:first_pts=0\n"

// and by default, no video filter
"video=null\n"

// discard audio in fast-forwarded sections
"ffclip=discard\n"

// formats and codecs to use
"\n[formats]\n"
"vformat=mkv\n"
"vcodec=libx264\n"
"vcrf=16\n"
"vbr=\n"
"aiformat=flac\n"
"aicodec=flac\n"
"aformat=wav\n"
"acodec=pcm_s16le\n"
"abr=\n"

// steps to perform in processing
"\n[steps]\n"
"noiser=n\n"
"aproc=2\n"

// Don't bypass normal video processing
"videobypass=\n"

// Track info, currently only which are included
"\n[tracks]\n"
"include=y\n" // include all tracks by default

// Instructions for marktofilter, currently only relating to fast-forwards
"\n[marktofilter]\n"
"fflen=8\n"
"minffspeed=4\n"
"maxffpitch=0\n"
"fffilter=null\n"
;
