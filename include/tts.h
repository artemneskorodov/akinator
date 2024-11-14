#ifndef TTS_H
#define TTS_H

#include <windows.h>
#include <sapi.h>
#include <iostream>
#include <Windows.h>

enum tts_error_t {
    TTS_SUCCESS = 0,
    TTS_ERROR   = 1,
};

static const size_t MaxMessageSize = 256;

struct tts_t {
    ISpVoice *speaker;
};

tts_error_t tts_ctor(tts_t *tts);
tts_error_t tts_dtor(tts_t *tts);
tts_error_t tts_speak(tts_t *tts, const char *string);

#endif
