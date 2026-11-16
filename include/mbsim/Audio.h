// mbsim: audio. No sound is produced; only the timing is simulated.
// API ported from codal-microbit-v2/inc/MicroBitAudio.h and SoundExpressions.h
// Copyright (c) 2016 British Broadcasting Corporation, (c) 2017 Lancaster University.
// MIT licence, see LICENSE.
#ifndef MBSIM_AUDIO_H
#define MBSIM_AUDIO_H

#include "CodalConstants.h"
#include "Component.h"
#include "ManagedString.h"

namespace codal
{
    class SoundExpressions
    {
        public:
        /**
          * Plays a sound and blocks the calling fiber until it has finished.
          * @param sound A built-in name ("giggle", "happy", "hello", "mysterious", "sad",
          *              "slide", "soaring", "spring", "twinkle", "yawn") or encoded sound data.
          */
        void play(ManagedString sound, uint16_t event = DEVICE_SOUND_EMOJI_SYNTHESIZER_EVT_DONE);

        /** Starts a sound and returns straight away. */
        void playAsync(ManagedString sound);

        /** Stops whatever is playing. */
        void stop();
    };

    class MicroBitAudio : public CodalComponent
    {
        public:
        SoundExpressions soundExpressions;

        MicroBitAudio();
        virtual void periodicCallback();

        /** @param volume 0 (muted) to 255 (loudest). */
        int setVolume(int volume);
        int getVolume();

        void setSpeakerEnabled(bool on);
        bool isSpeakerEnabled();
        void setPinEnabled(bool on);
        bool isPinEnabled();
        bool isPlaying();
        int enable();
        int disable();
    };
}

#endif
