/*
 * Copyright (c) 2020 Gregor Richards
 * Copyright (c) 2020 Alan Davison
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

#include <string>

#include <stdio.h>
#include <stdlib.h>

#include <obs-module.h>
#include <obs-frontend-api.h>

// This is a nasty hack for a lot of reasons
#define private private_
#include <obs-internal.h>
#undef private

#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QMainWindow>

OBS_DECLARE_MODULE()

struct obs_source_info plip_util;
static const char* pliputil_idname = "plip-util";

// UI stuff
static QWidget* handle;
static QStatusBar* statusbar;
static QLabel* monitorLabel;
#define MONITOR_LABEL_BLANK " Monitor ready "
static char monitorLabelText[32];

// UI style
#define BASE "font-size: 20px; background-color: #"
struct DisplayColor {
    const char *style;
    uint32_t raw;
    gs_texture_t *tex;
};
#define COLOR(nm, def) static struct DisplayColor color ## nm = { \
    BASE #def ";", \
    0xFF ## def, \
    NULL \
}
COLOR(Stopped, FF8888);
COLOR(Started, 88FF88);
COLOR(Mark, 8888FF);
COLOR(Restart, FF88FF);
COLOR(FastForward, FFFF88);
static struct DisplayColor colorNotMonitoring = {
    "",
    0xFF000000,
    NULL
};
#undef COLOR
#undef BASE

// Mark file we're writing to
static FILE* markFile;

// The file being written to, for timing purposes only
static obs_output_t* timeFile;

// The encoder from which we steal time info
static obs_encoder_t* timeEncoder;

// Time info
static double recTime; // absolute recording time
static double lastOutTime; // the output time as of the last time we stopped outputting
static double lastInTime; // the recording time as of the last time we started outputting

// Recording state
enum RecordingState {
    NOT_RECORDING,
    STOPPED,
    STARTED,
    FAST_FORWARD
};
static enum RecordingState curState;

// Display state
#define WIDTH 16
static gs_texture_t *curTexture;

static void changeState(enum RecordingState to, struct DisplayColor *colorOverride)
{
    if (curState != STARTED && to == STARTED) {
        // Started recording, remember our last in time
        lastInTime = recTime;
    }
    if (curState == STARTED && to != STARTED) {
        // Stopped recording, remember our last out time
        lastOutTime += recTime - lastInTime;
    }

    curState = to;

    // Now change the color
    struct DisplayColor *color = colorOverride;
    if (!color)
    {
        switch (to)
        {
            case NOT_RECORDING: color = &colorNotMonitoring; break;
            case STOPPED:       color = &colorStopped; break;
            case STARTED:       color = &colorStarted; break;
            case FAST_FORWARD:  color = &colorFastForward; break;
        }
    }
    if (color)
    {
        monitorLabel->setStyleSheet(color->style);
        if (!color->tex)
        {
            // Make a texture for it
            uint32_t *tex = (uint32_t *) malloc(WIDTH*WIDTH*4);
            if (tex)
            {
                for (int x = 0; x < WIDTH*WIDTH; x++)
                    tex[x] = color->raw;
                obs_enter_graphics();
                color->tex = gs_texture_create(WIDTH, WIDTH, GS_BGRA, 1,
                    (const uint8_t **) &tex, 0);
                obs_leave_graphics();
                free(tex);
            }
        }
        if (color->tex)
            curTexture = color->tex;
        else
            curTexture = NULL;
    }
}

// Called every video frame as a "tick"
static void pliputil_tick(void *data, float seconds)
{
    if (curState == NOT_RECORDING)
        return;

    if (!timeFile)
    {
        timeFile = obs_frontend_get_recording_output();
        if (!timeFile) return;
    }

    if (!timeEncoder)
    {
        // Try to get our encoder
        timeEncoder = obs_output_get_audio_encoder(timeFile, 0);
        if (!timeEncoder) return;
    }

    // Get the time from the encoder (NASTY INTERNALS: EXPECT BREAKAGE!)
    double rawRecTime = (((double) timeEncoder->cur_pts - timeFile->audio_offsets[0]) * timeEncoder->timebase_num) / timeEncoder->timebase_den;

    // Account for buffering delay (FIXME: More internals, hard-coding 48K)
    double bufferDelay = 0;
#ifndef _WIN32
    bufferDelay = obs->audio.total_buffering_ticks * AUDIO_OUTPUT_FRAMES / 48000.0;
#endif

    recTime = rawRecTime + bufferDelay;

    // Get the time to output
    double outTime = lastOutTime;
    if (curState == STARTED)
        outTime += recTime - lastInTime;

    // Turn it into textual time
    long h, m, s;
    s = outTime;
    m = s / 60;
    s %= 60;
    h = m / 60;
    m %= 60;
    snprintf(monitorLabelText, sizeof(monitorLabelText)-1, " MON: %ld:%02ld:%02ld ", h, m, s);
    if (monitorLabel)
        monitorLabel->setText(monitorLabelText);
}

// Output a mark to our file
static void output_mark(char mark)
{
    if (markFile)
    {
        fprintf(markFile, "%c%f\n", mark, recTime);
        fflush(markFile);
    }
}

static const char* pliputil_get_name(void*)
{
    return pliputil_idname;
}

void obs_module_unload(void) 
{
    if (monitorLabel)
        monitorLabel->deleteLater();
}

static void pliputil_destroy(void*) 
{
}

// Start/stop hotkey
static void pliputil_write_hotkey_1(void* data, obs_hotkey_id, obs_hotkey_t*, bool pressed)
{
    if (pressed)
    {
        char mk = 0;
        switch (curState)
        {
            case STARTED:
                mk = 'o'; // out
                changeState(STOPPED, NULL);
                break;

            case STOPPED:
                mk = 'i'; // in
                changeState(STARTED, NULL);
                break;

            case FAST_FORWARD:
                mk = 'n'; // normal
                changeState(STARTED, NULL);
                break;
        }

        if (mk)
            output_mark(mk);
    }
}

// Mark hotkey
static void pliputil_write_hotkey_2(void* data, obs_hotkey_id, obs_hotkey_t*, bool pressed)
{
    if (pressed)
    {
        output_mark('m'); // mark (of course)
        changeState(curState, &colorMark);
    }
    else
    {
        changeState(curState, NULL);
    }

}

// Fast-forward hotkey
static void pliputil_write_hotkey_3(void* data, obs_hotkey_id, obs_hotkey_t*, bool pressed)
{
    if (pressed)
    {
        if (curState == STARTED)
        {
            output_mark('f'); // fast forward
            changeState(FAST_FORWARD, NULL);
        }
    }
}

// Restart hotkey
static void pliputil_write_hotkey_4(void* data, obs_hotkey_id, obs_hotkey_t*, bool pressed)
{
    if (pressed)
    {
        if (curState == STOPPED)
        {
            output_mark('r'); // restart
            changeState(curState, &colorRestart);

            // Reset the visible part of our counter
            lastInTime = lastOutTime = 0;
        }
    }
    else
    {
        changeState(curState, NULL);
    }
}

static void pliputil_obs_event(enum obs_frontend_event event, void*)
{
    if (event == OBS_FRONTEND_EVENT_RECORDING_STARTED)
    {
        // Try to find our output file
        obs_output_t *recOutput = obs_frontend_get_recording_output();
        if (!recOutput) return;
        obs_data_t *recData = obs_output_get_settings(recOutput);
        if (!recData) return;
        const char *recPath = obs_data_get_string(recData, "path");
        if (!recPath) return;

        // Turn it into a mark file
        std::string markFileName = recPath;
        size_t lastDot = markFileName.find_last_of(".");
        markFileName = markFileName.substr(0, lastDot) + ".mark";
        markFile = fopen(markFileName.c_str(), "w"
#ifndef _WIN32
        "x"
#endif
        );
        if (!markFile) return;

        // We got our mark file, so we can start monitoring
        changeState(STOPPED, NULL);
        recTime = lastOutTime = lastInTime = 0;
    }
    if (event == OBS_FRONTEND_EVENT_RECORDING_STOPPED)
    {
        // Finalize any marking state
        switch (curState) {
            case FAST_FORWARD:
                output_mark('n');
                // fallthrough

            case STARTED:
                output_mark('o');
                break;
        }

        // Teardown
        changeState(NOT_RECORDING, NULL);
        if (monitorLabel)
            monitorLabel->setText(MONITOR_LABEL_BLANK);
        if (markFile)
        {
            fclose(markFile);
            markFile = NULL;
        }
        timeEncoder = NULL;
    }
}

static void* pliputil_create(obs_data_t *ignore, obs_source_t *source)
{
    obs_hotkey_register_source(source, "plip-util.key1", "Start/stop", pliputil_write_hotkey_1, NULL);
    obs_hotkey_register_source(source, "plip-util.key2", "Mark", pliputil_write_hotkey_2, NULL);
    obs_hotkey_register_source(source, "plip-util.key3", "Fast-forward", pliputil_write_hotkey_3, NULL);
    obs_hotkey_register_source(source, "plip-util.key4", "Restart", pliputil_write_hotkey_4, NULL);

    obs_frontend_add_event_callback(pliputil_obs_event, NULL);

    if (!monitorLabel) {
        handle = (QWidget*) obs_frontend_get_main_window();
        statusbar = handle->findChild<QStatusBar*>("statusbar");

        monitorLabel = new QLabel();
        monitorLabel->setText(MONITOR_LABEL_BLANK);
        statusbar->insertWidget(1, monitorLabel);

        changeState(NOT_RECORDING, NULL);
    }

    return source; // any old junk
}

// Stuff related to rendering the monitor state to video
static uint32_t pliputil_getwidth(void *data)
{
    return WIDTH;
}

static void pliputil_videorender(void *data, gs_effect_t *effect)
{
    gs_reset_blend_state();
    if (curTexture) {
        gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"),
            curTexture);
        gs_draw_sprite(curTexture, 0, WIDTH, WIDTH);
    }
}

void plip_util_setup()
{
    plip_util.id = "plip-util";
    plip_util.type = OBS_SOURCE_TYPE_INPUT;
    plip_util.output_flags = OBS_SOURCE_VIDEO;
    plip_util.get_name = pliputil_get_name;
    plip_util.create = pliputil_create;
    plip_util.destroy = pliputil_destroy;
    plip_util.get_width = pliputil_getwidth;
    plip_util.get_height = pliputil_getwidth;
    plip_util.video_render = pliputil_videorender;
    plip_util.video_tick = pliputil_tick;
    obs_register_source(&plip_util);
}

bool obs_module_load(void)
{
    plip_util_setup();
    return true;
}
