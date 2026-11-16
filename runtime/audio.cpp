// mbsim: sound expressions. No sound is produced; only how long each one takes.
// The expression format, the built-in sound data and the parsing order are ported from
// codal-microbit-v2/source/SoundExpressions.cpp; the "one sound at a time" behaviour is
// from SoundEmojiSynthesizer.cpp.
// Copyright (c) 2016 British Broadcasting Corporation, (c) 2017 Lancaster University.
// MIT licence, see LICENSE.

#include "internal.h"

using namespace codal;

namespace
{
    struct BuiltIn
    {
        const char *name;
        const char *data;
    };

    const BuiltIn builtIns[] = {
#include "sound_data.inc"
    };

    const uint16_t SYNTH_ID = DEVICE_ID_SOUND_EMOJI_SYNTHESIZER_0;

    // ---- state of the (single) synthesizer; simulation thread only
    bool playing = false;
    bool stopping = false;
    bool currentAsync = false;
    uint64_t startUs = 0;
    uint64_t endUs = 0;
    char currentName[48];
    int playbackCompleteIn = 0;     // ticks until PLAYBACK_COMPLETE
    mbsim::FiberLock synthLock = {nullptr, 1};
    bool callIsBlocking = false;    // set by play() around its playAsync()
    int volume = 255;
    bool speakerEnabled = true;
    bool pinEnabled = true;
}

std::atomic<int> mbsim::liveVolume{255};

// ---- parsing, as SoundExpressions.cpp

static int parseDigits(const char *input, const int digits)
{
    int result = 0;
    for (int i = 0; i < digits; ++i)
    {
        const int n = input[i] - '0';
        if (n < 0 || n > 9)
            return -1;
        const int powers = digits - i;
        switch (powers)
        {
            case 4: result += 1000 * n; break;
            case 3: result += 100 * n; break;
            case 2: result += 10 * n; break;
            case 1: result += n; break;
        }
    }
    return result;
}

static int applyRandom(int value, int rand)
{
    if (value < 0 || rand < 0)
        return -1;
    const int delta = codal::random(rand * 2 + 1) - rand;
    return abs(value + delta);
}

// Parses one 72 character effect. Only the duration matters to mbsim, but every field is
// read, and every random number drawn, in CODAL's order so that a program's later
// microbit_random() values are the ones the same seed would give after a real play().
static bool parseSoundExpression(const char *soundChars, int *durationMs)
{
    int wave = parseDigits(&soundChars[0], 1);
    int effectVolume = parseDigits(&soundChars[1], 4);
    int frequency = parseDigits(&soundChars[5], 4);
    int duration = parseDigits(&soundChars[9], 4);
    int shape = parseDigits(&soundChars[13], 2);
    int endFrequency = parseDigits(&soundChars[18], 4);
    int endVolume = parseDigits(&soundChars[26], 4);
    int steps = parseDigits(&soundChars[30], 4);
    int fxChoice = parseDigits(&soundChars[34], 2);
    int fxParam = parseDigits(&soundChars[36], 4);
    int fxnSteps = parseDigits(&soundChars[40], 4);
    (void)wave; (void)shape; (void)steps; (void)fxChoice;

    frequency = applyRandom(frequency, parseDigits(&soundChars[44], 4));
    endFrequency = applyRandom(endFrequency, parseDigits(&soundChars[48], 4));
    effectVolume = applyRandom(effectVolume, parseDigits(&soundChars[52], 4));
    endVolume = applyRandom(endVolume, parseDigits(&soundChars[56], 4));
    duration = applyRandom(duration, parseDigits(&soundChars[60], 4));
    fxParam = applyRandom(fxParam, parseDigits(&soundChars[64], 4));
    fxnSteps = applyRandom(fxnSteps, parseDigits(&soundChars[68], 4));

    if (frequency == -1 || endFrequency == -1 || effectVolume == -1 || endVolume == -1 || duration == -1 || fxParam == -1 || fxnSteps == -1)
        return false;

    *durationMs = duration;
    return true;
}

static const char *lookupBuiltIn(const char *sound)
{
    for (const BuiltIn &b : builtIns)
        if (strcmp(b.name, sound) == 0)
            return b.data;
    return sound;
}

// 72 characters per effect, comma separated. Checked without drawing random numbers.
static bool wellFormed(const char *soundChars)
{
    const unsigned soundLen = strlen(soundChars);
    const unsigned charsPerEffect = 72;
    const unsigned effectCount = (soundLen + 1) / (charsPerEffect + 1);
    const unsigned expectedLength = effectCount * (charsPerEffect + 1) - 1;
    if (effectCount == 0 || soundLen != expectedLength)
        return false;
    for (unsigned i = 0; i < soundLen; i++)
    {
        bool comma = (i % (charsPerEffect + 1)) == charsPerEffect;
        if (comma ? soundChars[i] != ',' : (soundChars[i] < '0' || soundChars[i] > '9'))
            return false;
    }
    return true;
}

static void finishSound()
{
    uint64_t now = mbsim::sim_now_us();
    mbsim::sound_out(currentName, startUs, now, currentAsync);
    mbsim::sound_status("", 0, false);
    playing = false;
    stopping = false;
    // SoundEmojiSynthesizer::pull(): flip our status bit and fire the event, then let the
    // next waiting play() in.
    playbackCompleteIn = 15;  // (CONFIG_EMOJI_SYNTHESIZER_OUTPUT_BUFFER_DEPTH + 2) buffers of 512 samples at 44.1 kHz ~ 58 ms
    Event(SYNTH_ID, DEVICE_SOUND_EMOJI_SYNTHESIZER_EVT_DONE);
    synthLock.notify();
}

void mbsim::audio_flush_at_exit()
{
    if (playing)
        sound_out(currentName, startUs, sim_now_us(), currentAsync);
}

MicroBitAudio::MicroBitAudio()
{
    this->id = DEVICE_ID_SYSTEM_DAC;
    this->status |= DEVICE_COMPONENT_STATUS_SYSTEM_TICK;
}

void MicroBitAudio::periodicCallback()
{
    if (playbackCompleteIn > 0 && --playbackCompleteIn == 0)
        Event(SYNTH_ID, DEVICE_SOUND_EMOJI_SYNTHESIZER_EVT_PLAYBACK_COMPLETE);

    if (playing && (stopping || mbsim::sim_now_us() >= endUs))
        finishSound();
}

int MicroBitAudio::setVolume(int v)
{
    if (v < 0 || v > 255)
        return DEVICE_INVALID_PARAMETER;
    volume = v;
    mbsim::liveVolume = v;
    return DEVICE_OK;
}

int MicroBitAudio::getVolume()
{
    return volume;
}

void MicroBitAudio::setSpeakerEnabled(bool on) { speakerEnabled = on; }
bool MicroBitAudio::isSpeakerEnabled() { return speakerEnabled; }
void MicroBitAudio::setPinEnabled(bool on) { pinEnabled = on; }
bool MicroBitAudio::isPinEnabled() { return pinEnabled; }
bool MicroBitAudio::isPlaying() { return playing; }
int MicroBitAudio::enable() { return DEVICE_OK; }
int MicroBitAudio::disable() { return DEVICE_OK; }

void SoundExpressions::play(ManagedString sound, uint16_t event)
{
    // DEVIATION: reading CODAL, play() with data that isn't
    // a valid expression registers its wake-up, plays nothing, and then waits for a DONE
    // event that never comes. mbsim warns and returns instead.
    if (!wellFormed(lookupBuiltIn(sound.toCharArray())))
    {
        mbsim::note("Unknown sound \"%.40s\": nothing played. (On the board, play() with an unknown "
                    "name may never return.)", sound.toCharArray());
        return;
    }

    fiber_wake_on_event(SYNTH_ID, event);
    callIsBlocking = true;
    playAsync(sound);
    callIsBlocking = false;
    schedule();
}

void SoundExpressions::playAsync(ManagedString sound)
{
    bool blocking = callIsBlocking;
    callIsBlocking = false;

    // Sound is either encoded data or a name of a built-in sound for which we have the data.
    const char *soundChars = lookupBuiltIn(sound.toCharArray());
    if (!wellFormed(soundChars))
    {
        mbsim::note("Unknown sound \"%.40s\": nothing played.", sound.toCharArray());
        return;
    }

    const unsigned charsPerEffect = 72;
    const unsigned effectCount = (strlen(soundChars) + 1) / (charsPerEffect + 1);
    uint64_t totalMs = 0;
    for (unsigned i = 0; i < effectCount; ++i)
    {
        int durationMs = 0;
        if (!parseSoundExpression(&soundChars[i * charsPerEffect + i], &durationMs))
            return;
        totalMs += (uint64_t)durationMs;
    }

    // SoundEmojiSynthesizer::play(): if a playout is already in progress, block until
    // it has finished.
    synthLock.wait();

    playing = true;
    stopping = false;
    currentAsync = !blocking;
    startUs = mbsim::sim_now_us();
    endUs = startUs + totalMs * 1000;
    snprintf(currentName, sizeof(currentName), "%s", soundChars == sound.toCharArray() ? "custom" : sound.toCharArray());
    mbsim::sound_status(currentName, endUs, true);
}

void SoundExpressions::stop()
{
    if (playing)
        stopping = true;
}
