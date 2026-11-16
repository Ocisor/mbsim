// mbsim: Image and BitmapFont, ported from codal-core/inc/types/Image.h and BitmapFont.h
// Copyright (c) 2017 Lancaster University. MIT licence, see LICENSE.
#ifndef MBSIM_IMAGE_H
#define MBSIM_IMAGE_H

#include <stdint.h>
#include <memory>
#include <vector>
#include "CodalConstants.h"
#include "ManagedString.h"

namespace codal
{
    class BitmapFont
    {
        public:
        static const unsigned char *defaultFont;
        static BitmapFont systemFont;

        const unsigned char *characters;
        int asciiEnd;

        BitmapFont(const unsigned char *font, int asciiEnd = BITMAP_FONT_ASCII_END);
        BitmapFont();

        static void setSystemFont(BitmapFont font);
        static BitmapFont getSystemFont();

        /** @return the 5 row bytes of the glyph for c, or NULL if c is outside ASCII 32-126. */
        const uint8_t *get(char c);
    };

    struct ImageData
    {
        int16_t width;
        int16_t height;
        std::vector<uint8_t> data;
        bool isDisplay;  // mbsim only: this is the buffer the LEDs show.
    };

    /**
      * A reference counted greyscale bitmap, one byte per pixel.
      * x is the column (0 = left), y is the row (0 = top).
      */
    class Image
    {
        std::shared_ptr<ImageData> ptr;
        void init(const int16_t x, const int16_t y, const uint8_t *bitmap);
        void changed();

        public:
        static Image EmptyImage;

        Image();
        Image(const Image &image);
        explicit Image(const char *s);
        Image(const int16_t x, const int16_t y);
        Image(const int16_t x, const int16_t y, const uint8_t *bitmap);
        ~Image();

        Image &operator=(const Image &i);
        bool operator==(const Image &i);

        /** Sets every pixel to 0. */
        void clear();

        /**
          * Sets the pixel at (x, y) to the given brightness.
          * @return DEVICE_OK, or DEVICE_INVALID_PARAMETER if (x, y) is outside the image.
          */
        int setPixelValue(int16_t x, int16_t y, uint8_t value);

        /** @return the pixel's value, or DEVICE_INVALID_PARAMETER if (x, y) is outside the image. */
        int getPixelValue(int16_t x, int16_t y);

        int printImage(int16_t x, int16_t y, const uint8_t *bitmap);
        int paste(const Image &image, int16_t x = 0, int16_t y = 0, uint8_t alpha = 0);
        int print(char c, int16_t x = 0, int16_t y = 0);

        int shiftLeft(int16_t n);
        int shiftRight(int16_t n);
        int shiftUp(int16_t n);
        int shiftDown(int16_t n);

        int getWidth() const { return ptr->width; }
        int getHeight() const { return ptr->height; }
        int getSize() const { return ptr->width * ptr->height; }
        uint8_t *getBitmap() const { return ptr->data.data(); }

        ManagedString toString();
        Image crop(int startx, int starty, int finx, int finy);
        bool isReadOnly() { return false; }
        Image clone();

        void mbsimMarkDisplay();  // mbsim internal
    };
}

#endif
