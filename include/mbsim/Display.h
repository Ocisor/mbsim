// mbsim: the 5x5 LED display. API ported from
//   codal-core/inc/drivers/LEDMatrix.h, AnimatedDisplay.h and
//   codal-microbit-v2/inc/MicroBitDisplay.h
// Copyright (c) 2017 Lancaster University. MIT licence, see LICENSE.
#ifndef MBSIM_DISPLAY_H
#define MBSIM_DISPLAY_H

#include "CodalConstants.h"
#include "Component.h"
#include "Image.h"
#include "ManagedString.h"

namespace codal
{
    enum AnimationMode
    {
        ANIMATION_MODE_NONE,
        ANIMATION_MODE_STOPPED,
        ANIMATION_MODE_SCROLL_TEXT,
        ANIMATION_MODE_PRINT_TEXT,
        ANIMATION_MODE_SCROLL_IMAGE,
        ANIMATION_MODE_ANIMATE_IMAGE,
        ANIMATION_MODE_ANIMATE_IMAGE_WITH_CLEAR,
        ANIMATION_MODE_PRINT_CHARACTER
    };

    enum DisplayMode
    {
        DISPLAY_MODE_BLACK_AND_WHITE,
        DISPLAY_MODE_GREYSCALE,
        DISPLAY_MODE_BLACK_AND_WHITE_LIGHT_SENSE,
        DISPLAY_MODE_GREYSCALE_LIGHT_SENSE
    };

    enum DisplayRotation
    {
        MATRIX_DISPLAY_ROTATION_0,
        MATRIX_DISPLAY_ROTATION_90,
        MATRIX_DISPLAY_ROTATION_180,
        MATRIX_DISPLAY_ROTATION_270
    };

    class MicroBitDisplay : public CodalComponent
    {
        // LED matrix state
        uint8_t brightness;
        DisplayMode mode;
        bool enabled;

        // AnimatedDisplay state
        AnimationMode animationMode;
        uint16_t animationDelay;
        uint16_t animationTick;
        ManagedString scrollingText;
        int16_t scrollingChar;
        uint8_t scrollingPosition;
        ManagedString printingText;
        int16_t printingChar;
        Image scrollingImage;
        int16_t scrollingImagePosition;
        int8_t scrollingImageStride;
        bool scrollingImageRendered;
        BitmapFont font;

        void animationUpdate();
        void updateScrollText();
        void updatePrintText();
        void updateScrollImage();
        void updateAnimateImage();
        void sendAnimationCompleteEvent();
        void waitForFreeDisplay();
        void fiberWait();

        public:
        /** The pixels the LEDs show. image.setPixelValue(x, y, v): x = column, y = row. */
        Image image;

        MicroBitDisplay(uint16_t id = DEVICE_ID_DISPLAY);
        ~MicroBitDisplay();

        virtual void periodicCallback();

        // ---- LED matrix -------------------------------------------------------------
        /**
          * DISPLAY_MODE_BLACK_AND_WHITE (the default): any non-zero pixel is fully lit.
          * DISPLAY_MODE_GREYSCALE: each pixel's value (0-255) sets its own brightness.
          */
        void setDisplayMode(DisplayMode mode);
        int getDisplayMode();
        void rotateTo(DisplayRotation rotation);
        void enable();
        void disable();
        void clear();
        int setBrightness(int b);
        int getBrightness();
        int getWidth();
        int getHeight();
        Image screenShot();
        int readLightLevel();
        void setSleep(bool doSleep);

        // ---- animations (AnimatedDisplay) ----------------------------------------------
        void stopAnimation();

        int printCharAsync(char c, int delay = 0);
        int printAsync(ManagedString s, int delay = DISPLAY_DEFAULT_PRINT_SPEED);
        int printAsync(Image i, int x = 0, int y = 0, int alpha = 0, int delay = 0);
        int printChar(char c, int delay = 0);
        int print(ManagedString s, int delay = DISPLAY_DEFAULT_PRINT_SPEED);
        int print(Image i, int x = 0, int y = 0, int alpha = 0, int delay = 0);

        int scrollAsync(ManagedString s, int delay = DISPLAY_DEFAULT_SCROLL_SPEED);
        int scrollAsync(Image image, int delay = DISPLAY_DEFAULT_SCROLL_SPEED, int stride = DISPLAY_DEFAULT_SCROLL_STRIDE);
        int scroll(ManagedString s, int delay = DISPLAY_DEFAULT_SCROLL_SPEED);
        int scroll(Image image, int delay = DISPLAY_DEFAULT_SCROLL_SPEED, int stride = DISPLAY_DEFAULT_SCROLL_STRIDE);

        int animateAsync(Image image, int delay, int stride, int startingPosition = DISPLAY_ANIMATE_DEFAULT_POS, int autoClear = DISPLAY_DEFAULT_AUTOCLEAR);
        int animate(Image image, int delay, int stride, int startingPosition = DISPLAY_ANIMATE_DEFAULT_POS, int autoClear = DISPLAY_DEFAULT_AUTOCLEAR);
    };
}

#endif
