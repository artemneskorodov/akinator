#include "tts.h"

static tts_error_t ConvertCP1251ToUTF8(const char *string, wchar_t *output);

tts_error_t ConvertCP1251ToUTF8(const char *string, wchar_t *output) {
    int len = MultiByteToWideChar(1251, 0, string, -1, NULL, 0);
    MultiByteToWideChar(1251, 0, string, -1, output, len);

    return TTS_SUCCESS;
}

tts_error_t tts_ctor(tts_t *tts) {
    if(SUCCEEDED(::CoInitialize(0))) {
        ISpVoice* pVoice = nullptr;
        const HRESULT hr_create = ::CoCreateInstance(::CLSID_SpVoice,
                                                     nullptr,
                                                     CLSCTX_ALL,
                                                     ::IID_ISpVoice,
                                                     (void **)&pVoice);
        pVoice->SetRate(5);
        if(!SUCCEEDED(hr_create)) {
            return TTS_ERROR;
        }
        tts->speaker = pVoice;
    }
    return TTS_SUCCESS;
}

tts_error_t tts_dtor(tts_t *tts) {
    tts->speaker->Release();
    tts->speaker = NULL;
    ::CoUninitialize();
    return TTS_SUCCESS;
}

tts_error_t tts_speak(tts_t *tts, const char *string) {
    wchar_t wide_string[MaxMessageSize + 1] = {};
    if(ConvertCP1251ToUTF8(string, wide_string) != TTS_SUCCESS) {
        return TTS_ERROR;
    }
    const HRESULT hr_speak = tts->speaker->Speak(wide_string, 0, nullptr);
    if(!SUCCEEDED(hr_speak)) {
        return TTS_ERROR;
    }
    return TTS_SUCCESS;
}
