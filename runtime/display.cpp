// mbsim: Image, the pendolino3 font, display modes and display animations.
// Ported from codal-core/source/types/Image.cpp, types/BitmapFont.cpp,
// drivers/AnimatedDisplay.cpp and the brightness / mode parts of drivers/LEDMatrix.cpp.
// Copyright (c) 2017 Lancaster University. MIT licence, see LICENSE.

#include "internal.h"

#include <ctype.h>

using namespace codal;

// -------------------------------------------------------------------------------- font

static const unsigned char pendolino3[475] = {
#include "pendolino3.inc"
};

const unsigned char *BitmapFont::defaultFont = pendolino3;
BitmapFont BitmapFont::systemFont = BitmapFont(pendolino3, BITMAP_FONT_ASCII_END);

BitmapFont::BitmapFont(const unsigned char *characters, int asciiEnd)
{
    this->characters = characters;
    this->asciiEnd = asciiEnd;
}

BitmapFont::BitmapFont()
{
    this->characters = pendolino3;
    this->asciiEnd = BITMAP_FONT_ASCII_END;
}

void BitmapFont::setSystemFont(BitmapFont font)
{
    BitmapFont::systemFont = font;
}

BitmapFont BitmapFont::getSystemFont()
{
    // The user's global uBit may be built before systemFont is; fall back to the default.
    if (BitmapFont::systemFont.characters == NULL)
        return BitmapFont();
    return BitmapFont::systemFont;
}

const uint8_t *BitmapFont::get(char c)
{
    if (c < BITMAP_FONT_ASCII_START || c > BITMAP_FONT_ASCII_END)
        return NULL;

    return characters + (c - BITMAP_FONT_ASCII_START) * ((1 + (BITMAP_FONT_WIDTH / 8)) * BITMAP_FONT_HEIGHT);
}

// ------------------------------------------------------------------ live display state

std::atomic<uint8_t> mbsim::livePixels[25];
std::atomic<int> mbsim::liveMode{DISPLAY_MODE_BLACK_AND_WHITE};
std::atomic<int> mbsim::liveBrightness{255};
std::atomic<bool> mbsim::liveEnabled{true};

namespace
{
    const uint8_t *displayPixels = nullptr;  // the display image's buffer
    bool frameWritten = false;
    uint8_t lastLit[25];
    int lastMode = -1;
    int lastBrightness = -1;
}

void mbsim::display_register(const uint8_t *pixels)
{
    displayPixels = pixels;
}

void mbsim::display_changed(const uint8_t *pixels)
{
    for (int i = 0; i < 25; i++)
        livePixels[i].store(pixels[i], std::memory_order_relaxed);

    if (started && !fiber_scheduler_running())
        note_once("display-before-init",
                  "The display was used before uBit.init(). Call uBit.init() first.");
}

int mbsim::lit_level(int pixel, int mode, int brightness)
{
    if (pixel == 0)
        return 0;
    if (mode == DISPLAY_MODE_GREYSCALE || mode == DISPLAY_MODE_GREYSCALE_LIGHT_SENSE)
        return (pixel * brightness + 127) / 255;
    return brightness;  // black and white: any non-zero pixel is fully on
}

void mbsim::flush_frame()
{
    // Pick up writes made straight into the bitmap (getBitmap()).
    if (displayPixels)
        for (int i = 0; i < 25; i++)
            livePixels[i].store(displayPixels[i], std::memory_order_relaxed);

    if (!opt().headless || !started)
        return;

    int mode = liveMode.load();
    int brightness = liveBrightness.load();
    bool enabled = liveEnabled.load();
    uint8_t lit[25];
    for (int i = 0; i < 25; i++)
        lit[i] = enabled ? (uint8_t)lit_level(livePixels[i].load(std::memory_order_relaxed), mode, brightness) : 0;

    if (frameWritten && mode == lastMode && brightness == lastBrightness && memcmp(lit, lastLit, 25) == 0)
        return;

    frameWritten = true;
    lastMode = mode;
    lastBrightness = brightness;
    memcpy(lastLit, lit, 25);
    headless_frame(sim_now_us() / 1000, mode, brightness, lit);
}

// ------------------------------------------------------------------------------- Image

Image Image::EmptyImage;

void Image::init(const int16_t x, const int16_t y, const uint8_t *bitmap)
{
    ptr = std::make_shared<ImageData>();
    ptr->width = x > 0 ? x : 0;
    ptr->height = y > 0 ? y : 0;
    ptr->isDisplay = false;
    // One spare byte so getBitmap() is never NULL, even for an empty image.
    ptr->data.assign((size_t)ptr->width * ptr->height + 1, 0);

    if (bitmap)
        this->printImage(x, y, bitmap);
}

void Image::changed()
{
    if (ptr->isDisplay)
        mbsim::display_changed(ptr->data.data());
}

void Image::mbsimMarkDisplay()
{
    ptr->isDisplay = true;
    mbsim::display_register(ptr->data.data());
}

Image::Image()
{
    init(0, 0, NULL);
}

Image::Image(const Image &image)
{
    ptr = image.ptr;
}

Image::Image(const char *s)
{
    int width = 0;
    int height = 0;
    int count = 0;
    int digit = 0;

    char parseBuf[10];

    const char *parseReadPtr;
    char *parseWritePtr;
    uint8_t *bitmapPtr;

    if (s == NULL)
    {
        init(0, 0, NULL);
        return;
    }

    // First pass: Parse the string to determine the geometry of the image.
    parseReadPtr = s;

    while (*parseReadPtr)
    {
        if (isdigit((unsigned char)*parseReadPtr))
        {
            // Ignore numbers.
            digit = 1;
        }
        else if (*parseReadPtr == '\n')
        {
            if (digit)
            {
                count++;
                digit = 0;
            }

            height++;

            width = count > width ? count : width;
            count = 0;
        }
        else
        {
            if (digit)
            {
                count++;
                digit = 0;
            }
        }

        parseReadPtr++;
    }

    this->init(width, height, NULL);

    // Second pass: collect the data.
    parseReadPtr = s;
    parseWritePtr = parseBuf;
    bitmapPtr = this->getBitmap();
    uint8_t *bitmapEnd = bitmapPtr + getSize();

    while (*parseReadPtr)
    {
        if (isdigit((unsigned char)*parseReadPtr))
        {
            if (parseWritePtr < parseBuf + sizeof(parseBuf) - 1)
            {
                *parseWritePtr = *parseReadPtr;
                parseWritePtr++;
            }
        }
        else
        {
            *parseWritePtr = 0;
            if (parseWritePtr > parseBuf)
            {
                if (bitmapPtr < bitmapEnd)
                {
                    *bitmapPtr = atoi(parseBuf);
                    bitmapPtr++;
                }
                parseWritePtr = parseBuf;
            }
        }

        parseReadPtr++;
    }
}

Image::Image(const int16_t x, const int16_t y)
{
    this->init(x, y, NULL);
}

Image::Image(const int16_t x, const int16_t y, const uint8_t *bitmap)
{
    this->init(x, y, bitmap);
}

Image::~Image()
{
}

Image &Image::operator=(const Image &i)
{
    if (ptr == i.ptr)
        return *this;

    if (ptr && ptr->isDisplay)
    {
        // CODAL would repoint the display at the other image's shared buffer. mbsim
        // keeps one fixed display buffer, so copy the pixels across instead.
        Image src(i);
        clear();
        paste(src, 0, 0, 0);
        return *this;
    }

    ptr = i.ptr;
    return *this;
}

bool Image::operator==(const Image &i)
{
    if (ptr == i.ptr)
        return true;
    else
        return (ptr->width == i.ptr->width && ptr->height == i.ptr->height && (memcmp(getBitmap(), i.getBitmap(), getSize()) == 0));
}

void Image::clear()
{
    memset(getBitmap(), 0, getSize());
    changed();
}

int Image::setPixelValue(int16_t x, int16_t y, uint8_t value)
{
    // sanity check
    if (x >= getWidth() || y >= getHeight() || x < 0 || y < 0)
        return DEVICE_INVALID_PARAMETER;

    this->getBitmap()[y * getWidth() + x] = value;
    changed();
    return DEVICE_OK;
}

int Image::getPixelValue(int16_t x, int16_t y)
{
    // sanity check
    if (x >= getWidth() || y >= getHeight() || x < 0 || y < 0)
        return DEVICE_INVALID_PARAMETER;

    return this->getBitmap()[y * getWidth() + x];
}

int Image::printImage(int16_t width, int16_t height, const uint8_t *bitmap)
{
    const uint8_t *pIn;
    uint8_t *pOut;
    int pixelsToCopyX, pixelsToCopyY;

    // Sanity check.
    if (width <= 0 || width <= 0 || bitmap == NULL)
        return DEVICE_INVALID_PARAMETER;

    // Calcualte sane start pointer.
    pixelsToCopyX = min(width, this->getWidth());
    pixelsToCopyY = min(height, this->getHeight());

    pIn = bitmap;
    pOut = this->getBitmap();

    // Copy the image, stride by stride.
    for (int i = 0; i < pixelsToCopyY; i++)
    {
        memcpy(pOut, pIn, pixelsToCopyX);
        pIn += width;
        pOut += this->getWidth();
    }

    changed();
    return DEVICE_OK;
}

int Image::paste(const Image &image, int16_t x, int16_t y, uint8_t alpha)
{
    uint8_t *pIn, *pOut;
    int cx, cy;
    int pxWritten = 0;

    // Sanity check.
    // We permit writes that overlap us, but ones that are clearly out of scope we can filter early.
    if (x >= getWidth() || y >= getHeight() || x + image.getWidth() <= 0 || y + image.getHeight() <= 0)
        return 0;

    // Calculate the number of byte we need to copy in each dimension.
    cx = x < 0 ? min(image.getWidth() + x, getWidth()) : min(image.getWidth(), getWidth() - x);
    cy = y < 0 ? min(image.getHeight() + y, getHeight()) : min(image.getHeight(), getHeight() - y);

    // Calculate sane start pointer.
    pIn = image.getBitmap();
    pIn += (x < 0) ? -x : 0;
    pIn += (y < 0) ? -image.getWidth() * y : 0;

    pOut = getBitmap();
    pOut += (x > 0) ? x : 0;
    pOut += (y > 0) ? getWidth() * y : 0;

    // Copy the image, stride by stride
    // If we want primitive transparecy, we do this byte by byte.
    // If we don't, use a more efficient block memory copy instead. Every little helps!

    if (alpha)
    {
        for (int i = 0; i < cy; i++)
        {
            for (int j = 0; j < cx; j++)
            {
                // Copy this byte if appropriate.
                if (*(pIn + j) != 0)
                {
                    *(pOut + j) = *(pIn + j);
                    pxWritten++;
                }
            }

            pIn += image.getWidth();
            pOut += getWidth();
        }
    }
    else
    {
        for (int i = 0; i < cy; i++)
        {
            memmove(pOut, pIn, cx);

            pxWritten += cx;
            pIn += image.getWidth();
            pOut += getWidth();
        }
    }

    changed();
    return pxWritten;
}

int Image::print(char c, int16_t x, int16_t y)
{
    const uint8_t *v;
    int x1, y1;

    BitmapFont font = BitmapFont::getSystemFont();

    // Sanity check. Silently ignore anything out of bounds.
    if (x >= getWidth() || y >= getHeight() || c < BITMAP_FONT_ASCII_START || c > font.asciiEnd)
        return DEVICE_INVALID_PARAMETER;

    // Paste.
    v = font.get(c);

    for (int row = 0; row < BITMAP_FONT_HEIGHT; row++)
    {
        // Update our Y co-ord write position
        y1 = y + row;

        for (int col = 0; col < BITMAP_FONT_WIDTH; col++)
        {
            // Update our X co-ord write position
            x1 = x + col;

            if (x1 >= 0 && y1 >= 0 && x1 < getWidth() && y1 < getHeight())
                this->getBitmap()[y1 * getWidth() + x1] = ((*v) & (0x10 >> col)) ? 255 : 0;
        }
        v++;
    }

    changed();
    return DEVICE_OK;
}

int Image::shiftLeft(int16_t n)
{
    uint8_t *p = getBitmap();
    int pixels = getWidth() - n;

    if (n <= 0)
        return DEVICE_INVALID_PARAMETER;

    if (n >= getWidth())
    {
        clear();
        return DEVICE_OK;
    }

    for (int y = 0; y < getHeight(); y++)
    {
        // Copy, and blank fill the rightmost column.
        memmove(p, p + n, pixels);
        memset(p + pixels, 0, n);
        p += getWidth();
    }

    changed();
    return DEVICE_OK;
}

int Image::shiftRight(int16_t n)
{
    uint8_t *p = getBitmap();
    int pixels = getWidth() - n;

    if (n <= 0)
        return DEVICE_INVALID_PARAMETER;

    if (n >= getWidth())
    {
        clear();
        return DEVICE_OK;
    }

    for (int y = 0; y < getHeight(); y++)
    {
        // Copy, and blank fill the leftmost column.
        memmove(p + n, p, pixels);
        memset(p, 0, n);
        p += getWidth();
    }

    changed();
    return DEVICE_OK;
}

int Image::shiftUp(int16_t n)
{
    uint8_t *pOut, *pIn;

    if (n <= 0)
        return DEVICE_INVALID_PARAMETER;

    if (n >= getHeight())
    {
        clear();
        return DEVICE_OK;
    }

    pOut = getBitmap();
    pIn = getBitmap() + getWidth() * n;

    for (int y = 0; y < getHeight(); y++)
    {
        // Copy, and blank fill the leftmost column.
        if (y < getHeight() - n)
            memcpy(pOut, pIn, getWidth());
        else
            memset(pOut, 0, getWidth());

        pIn += getWidth();
        pOut += getWidth();
    }

    changed();
    return DEVICE_OK;
}

int Image::shiftDown(int16_t n)
{
    uint8_t *pOut, *pIn;

    if (n <= 0)
        return DEVICE_INVALID_PARAMETER;

    if (n >= getHeight())
    {
        clear();
        return DEVICE_OK;
    }

    pOut = getBitmap() + getWidth() * (getHeight() - 1);
    pIn = pOut - getWidth() * n;

    for (int y = 0; y < getHeight(); y++)
    {
        // Copy, and blank fill the leftmost column.
        if (y < getHeight() - n)
            memcpy(pOut, pIn, getWidth());
        else
            memset(pOut, 0, getWidth());

        pIn -= getWidth();
        pOut -= getWidth();
    }

    changed();
    return DEVICE_OK;
}

ManagedString Image::toString()
{
    // width * height * (up to 3 digits + separator)
    std::string out;
    uint8_t *bitmapPtr = getBitmap();

    for (int row = 0; row < getHeight(); row++)
    {
        for (int col = 0; col < getWidth(); col++)
        {
            out += std::to_string((int)*bitmapPtr);
            out += ',';
            bitmapPtr++;
        }
        if (!out.empty())
            out.back() = '\n';
    }

    return ManagedString(out.c_str());
}

Image Image::crop(int startx, int starty, int cropWidth, int cropHeight)
{
    int newWidth = getWidth() - startx;
    int newHeight = getHeight() - starty;

    if (newWidth >= cropWidth)
        newWidth = cropWidth;

    if (newHeight >= cropHeight)
        newHeight = cropHeight;

    if (startx < 0 || starty < 0 || newWidth <= 0 || newHeight <= 0)
        return Image();

    Image result(newWidth, newHeight);
    for (int y = 0; y < newHeight; y++)
        for (int x = 0; x < newWidth; x++)
            result.getBitmap()[y * newWidth + x] = getBitmap()[(starty + y) * getWidth() + startx + x];

    return result;
}

Image Image::clone()
{
    return Image(getWidth(), getHeight(), getBitmap());
}

// --------------------------------------------------------------------- MicroBitDisplay

MicroBitDisplay::MicroBitDisplay(uint16_t id) : image(5, 5)
{
    this->id = id;
    this->status = 0;

    brightness = 255;
    mode = DISPLAY_MODE_BLACK_AND_WHITE;
    enabled = true;

    status |= DEVICE_COMPONENT_STATUS_SYSTEM_TICK;
    status |= DEVICE_COMPONENT_RUNNING;

    animationMode = ANIMATION_MODE_NONE;
    animationDelay = 0;
    animationTick = 0;
    scrollingChar = 0;
    scrollingPosition = 0;
    printingChar = 0;
    scrollingImagePosition = 0;
    scrollingImageStride = 0;
    scrollingImageRendered = false;

    image.mbsimMarkDisplay();
    mbsim::liveMode = mode;
    mbsim::liveBrightness = brightness;
    mbsim::liveEnabled = true;
}

MicroBitDisplay::~MicroBitDisplay()
{
    status &= ~DEVICE_COMPONENT_STATUS_SYSTEM_TICK;
    mbsim::display_register(nullptr);
}

void MicroBitDisplay::setDisplayMode(DisplayMode mode)
{
    this->mode = mode;
    mbsim::liveMode = mode;
    if (mode == DISPLAY_MODE_BLACK_AND_WHITE_LIGHT_SENSE || mode == DISPLAY_MODE_GREYSCALE_LIGHT_SENSE)
        mbsim::notSimulated("light sensing (the display still works)");
}

int MicroBitDisplay::getDisplayMode()
{
    return this->mode;
}

void MicroBitDisplay::rotateTo(DisplayRotation)
{
    mbsim::notSimulated("display rotation");
}

void MicroBitDisplay::enable()
{
    enabled = true;
    mbsim::liveEnabled = true;
}

void MicroBitDisplay::disable()
{
    enabled = false;
    mbsim::liveEnabled = false;
}

void MicroBitDisplay::clear()
{
    image.clear();
}

int MicroBitDisplay::setBrightness(int b)
{
    // sanitise the brightness level
    if (b < 0 || b > 255)
        return DEVICE_INVALID_PARAMETER;

    this->brightness = b;
    mbsim::liveBrightness = b;

    return DEVICE_OK;
}

int MicroBitDisplay::getBrightness()
{
    return this->brightness;
}

int MicroBitDisplay::getWidth()
{
    return 5;
}

int MicroBitDisplay::getHeight()
{
    return 5;
}

Image MicroBitDisplay::screenShot()
{
    return image.crop(0, 0, 5, 5);
}

int MicroBitDisplay::readLightLevel()
{
    return mbsim::notSimulated("the light sensor");
}

void MicroBitDisplay::setSleep(bool doSleep)
{
    if (doSleep)
        disable();
    else
        enable();
}

// ---- AnimatedDisplay

void MicroBitDisplay::periodicCallback()
{
    this->animationUpdate();
}

void MicroBitDisplay::animationUpdate()
{
    // If there's no ongoing animation, then nothing to do.
    if (animationMode == ANIMATION_MODE_NONE)
        return;

    animationTick += (SCHEDULER_TICK_PERIOD_US / 1000);

    if (animationTick >= animationDelay)
    {
        animationTick = 0;

        if (animationMode == ANIMATION_MODE_SCROLL_TEXT)
            this->updateScrollText();

        if (animationMode == ANIMATION_MODE_PRINT_TEXT)
            this->updatePrintText();

        if (animationMode == ANIMATION_MODE_SCROLL_IMAGE)
            this->updateScrollImage();

        if (animationMode == ANIMATION_MODE_ANIMATE_IMAGE || animationMode == ANIMATION_MODE_ANIMATE_IMAGE_WITH_CLEAR)
            this->updateAnimateImage();

        if (animationMode == ANIMATION_MODE_PRINT_CHARACTER)
        {
            animationMode = ANIMATION_MODE_NONE;
            this->sendAnimationCompleteEvent();
        }
    }
}

void MicroBitDisplay::sendAnimationCompleteEvent()
{
    // Signal that we've completed an animation.
    Event(id, DISPLAY_EVT_ANIMATION_COMPLETE);

    // Wake up a fiber that was blocked on the animation (if any).
    Event(DEVICE_ID_NOTIFY_ONE, DISPLAY_EVT_FREE);
}

void MicroBitDisplay::updateScrollText()
{
    image.shiftLeft(1);

    if (scrollingPosition < BITMAP_FONT_WIDTH && scrollingChar < scrollingText.length())
    {
        const uint8_t *v = font.get(scrollingText.charAt(scrollingChar));
        if (v == NULL)
            v = font.get(' ');

        uint8_t mask = 1 << (BITMAP_FONT_WIDTH - scrollingPosition - 1);
        uint8_t x = getWidth() - 1;

        for (int y = 0; y < BITMAP_FONT_HEIGHT; y++)
        {
            if (*v & mask)
                image.setPixelValue(x, y, 255);
            v++;
        }
    }

    scrollingPosition++;

    if (scrollingPosition == getWidth() + DISPLAY_SPACING)
    {
        scrollingPosition = 0;

        if (scrollingChar >= scrollingText.length())
        {
            animationMode = ANIMATION_MODE_NONE;
            this->sendAnimationCompleteEvent();
            return;
        }
        scrollingChar++;
    }
}

void MicroBitDisplay::updatePrintText()
{
    image.print(printingChar < printingText.length() ? printingText.charAt(printingChar) : ' ', 0, 0);

    if (printingChar > printingText.length())
    {
        animationMode = ANIMATION_MODE_NONE;

        this->sendAnimationCompleteEvent();
        return;
    }

    printingChar++;
}

void MicroBitDisplay::updateScrollImage()
{
    image.clear();

    if (((image.paste(scrollingImage, scrollingImagePosition, 0, 0) == 0) && scrollingImageRendered) || scrollingImageStride == 0)
    {
        animationMode = ANIMATION_MODE_NONE;
        this->sendAnimationCompleteEvent();

        return;
    }

    scrollingImagePosition += scrollingImageStride;
    scrollingImageRendered = true;
}

void MicroBitDisplay::updateAnimateImage()
{
    // wait until we have rendered the last position to give a continuous animation.
    if (scrollingImagePosition <= -scrollingImage.getWidth() + (getWidth() + scrollingImageStride) && scrollingImageRendered)
    {
        if (animationMode == ANIMATION_MODE_ANIMATE_IMAGE_WITH_CLEAR)
            this->image.clear();

        animationMode = ANIMATION_MODE_NONE;

        this->sendAnimationCompleteEvent();
        return;
    }

    if (scrollingImagePosition > 0)
        image.shiftLeft(-scrollingImageStride);

    image.paste(scrollingImage, scrollingImagePosition, 0, 0);

    if (scrollingImageStride == 0)
    {
        animationMode = ANIMATION_MODE_NONE;
        this->sendAnimationCompleteEvent();
    }

    scrollingImageRendered = true;

    scrollingImagePosition += scrollingImageStride;
}

void MicroBitDisplay::stopAnimation()
{
    // Reset any ongoing animation.
    if (animationMode != ANIMATION_MODE_NONE)
    {
        animationMode = ANIMATION_MODE_NONE;

        // Indicate that we've completed an animation.
        Event(id, DISPLAY_EVT_ANIMATION_COMPLETE);

        // Wake up aall fibers that may blocked on the animation (if any).
        Event(DEVICE_ID_NOTIFY, DISPLAY_EVT_FREE);
    }

    // Clear the display and setup the animation timers.
    this->image.clear();
}

void MicroBitDisplay::waitForFreeDisplay()
{
    // If there's an ongoing animation, wait for our turn to display.
    while (animationMode != ANIMATION_MODE_NONE && animationMode != ANIMATION_MODE_STOPPED)
    {
        if (fiber_wait_for_event(DEVICE_ID_NOTIFY, DISPLAY_EVT_FREE) == DEVICE_NOT_SUPPORTED)
            mbsim::spin_wait_us(1000);  // scheduler not running: nothing to wait on but time
    }
}

void MicroBitDisplay::fiberWait()
{
    if (fiber_wait_for_event(DEVICE_ID_DISPLAY, DISPLAY_EVT_ANIMATION_COMPLETE) == DEVICE_NOT_SUPPORTED)
        while (animationMode != ANIMATION_MODE_NONE && animationMode != ANIMATION_MODE_STOPPED)
            mbsim::spin_wait_us(1000);  // target_wait_for_event()
}

int MicroBitDisplay::printCharAsync(char c, int delay)
{
    // sanitise this value
    if (delay < 0)
        return DEVICE_INVALID_PARAMETER;

    // If the display is free, it's our turn to display.
    if (animationMode == ANIMATION_MODE_NONE || animationMode == ANIMATION_MODE_STOPPED)
    {
        image.print(c, 0, 0);

        if (delay > 0)
        {
            animationDelay = delay;
            animationTick = 0;
            animationMode = ANIMATION_MODE_PRINT_CHARACTER;
        }
    }
    else
    {
        return DEVICE_BUSY;
    }

    return DEVICE_OK;
}

int MicroBitDisplay::printAsync(ManagedString s, int delay)
{
    if (s.length() == 1)
        return printCharAsync(s.charAt(0));

    // sanitise this value
    if (delay <= 0)
        return DEVICE_INVALID_PARAMETER;

    if (animationMode == ANIMATION_MODE_NONE || animationMode == ANIMATION_MODE_STOPPED)
    {
        printingChar = 0;
        printingText = s;
        animationDelay = delay;
        animationTick = 0;

        animationMode = ANIMATION_MODE_PRINT_TEXT;
    }
    else
    {
        return DEVICE_BUSY;
    }

    return DEVICE_OK;
}

int MicroBitDisplay::printAsync(Image i, int x, int y, int alpha, int delay)
{
    if (delay < 0)
        return DEVICE_INVALID_PARAMETER;

    if (animationMode == ANIMATION_MODE_NONE || animationMode == ANIMATION_MODE_STOPPED)
    {
        image.paste(i, x, y, alpha);

        if (delay > 0)
        {
            animationDelay = delay;
            animationTick = 0;
            animationMode = ANIMATION_MODE_PRINT_CHARACTER;
        }
    }
    else
    {
        return DEVICE_BUSY;
    }

    return DEVICE_OK;
}

int MicroBitDisplay::printChar(char c, int delay)
{
    if (delay < 0)
        return DEVICE_INVALID_PARAMETER;

    // If there's an ongoing animation, wait for our turn to display.
    this->waitForFreeDisplay();

    // If the display is free, it's our turn to display.
    // If someone called stopAnimation(), then we simply skip...
    if (animationMode == ANIMATION_MODE_NONE)
    {
        this->printCharAsync(c, delay);

        if (delay > 0)
            fiberWait();
    }
    else
    {
        return DEVICE_CANCELLED;
    }

    return DEVICE_OK;
}

int MicroBitDisplay::print(ManagedString s, int delay)
{
    // sanitise this value
    if (delay <= 0)
        return DEVICE_INVALID_PARAMETER;

    // If there's an ongoing animation, wait for our turn to display.
    this->waitForFreeDisplay();

    // If the display is free, it's our turn to display.
    // If someone called stopAnimation(), then we simply skip...
    if (animationMode == ANIMATION_MODE_NONE)
    {
        if (s.length() == 1)
        {
            return printCharAsync(s.charAt(0));
        }
        else
        {
            this->printAsync(s, delay);
            fiberWait();
        }
    }
    else
    {
        return DEVICE_CANCELLED;
    }

    return DEVICE_OK;
}

int MicroBitDisplay::print(Image i, int x, int y, int alpha, int delay)
{
    if (delay < 0)
        return DEVICE_INVALID_PARAMETER;

    // If there's an ongoing animation, wait for our turn to display.
    this->waitForFreeDisplay();

    // If the display is free, it's our turn to display.
    // If someone called stopAnimation(), then we simply skip...
    if (animationMode == ANIMATION_MODE_NONE)
    {
        this->printAsync(i, x, y, alpha, delay);

        if (delay > 0)
            fiberWait();
    }
    else
    {
        return DEVICE_CANCELLED;
    }

    return DEVICE_OK;
}

int MicroBitDisplay::scrollAsync(ManagedString s, int delay)
{
    // sanitise this value
    if (delay <= 0)
        return DEVICE_INVALID_PARAMETER;

    // If the display is free, it's our turn to display.
    if (animationMode == ANIMATION_MODE_NONE || animationMode == ANIMATION_MODE_STOPPED)
    {
        scrollingPosition = 0;
        scrollingChar = 0;
        scrollingText = s;

        animationDelay = delay;
        animationTick = 0;
        animationMode = ANIMATION_MODE_SCROLL_TEXT;
    }
    else
    {
        return DEVICE_BUSY;
    }

    return DEVICE_OK;
}

int MicroBitDisplay::scrollAsync(Image image, int delay, int stride)
{
    // sanitise the delay value
    if (delay <= 0)
        return DEVICE_INVALID_PARAMETER;

    // If the display is free, it's our turn to display.
    if (animationMode == ANIMATION_MODE_NONE || animationMode == ANIMATION_MODE_STOPPED)
    {
        scrollingImagePosition = stride < 0 ? getWidth() : -image.getWidth();
        scrollingImageStride = stride;
        scrollingImage = image;
        scrollingImageRendered = false;

        animationDelay = stride == 0 ? 0 : delay;
        animationTick = 0;
        animationMode = ANIMATION_MODE_SCROLL_IMAGE;
    }
    else
    {
        return DEVICE_BUSY;
    }

    return DEVICE_OK;
}

int MicroBitDisplay::scroll(ManagedString s, int delay)
{
    // sanitise this value
    if (delay <= 0)
        return DEVICE_INVALID_PARAMETER;

    // If there's an ongoing animation, wait for our turn to display.
    this->waitForFreeDisplay();

    // If the display is free, it's our turn to display.
    // If someone called stopAnimation(), then we simply skip...
    if (animationMode == ANIMATION_MODE_NONE)
    {
        // Start the effect.
        this->scrollAsync(s, delay);

        // Wait for completion.
        fiberWait();
    }
    else
    {
        return DEVICE_CANCELLED;
    }

    return DEVICE_OK;
}

int MicroBitDisplay::scroll(Image image, int delay, int stride)
{
    // sanitise the delay value
    if (delay <= 0)
        return DEVICE_INVALID_PARAMETER;

    // If there's an ongoing animation, wait for our turn to display.
    this->waitForFreeDisplay();

    // If the display is free, it's our turn to display.
    // If someone called stopAnimation(), then we simply skip...
    if (animationMode == ANIMATION_MODE_NONE)
    {
        // Start the effect.
        this->scrollAsync(image, delay, stride);

        // Wait for completion.
        fiberWait();
    }
    else
    {
        return DEVICE_CANCELLED;
    }

    return DEVICE_OK;
}

int MicroBitDisplay::animateAsync(Image image, int delay, int stride, int startingPosition, int autoClear)
{
    // sanitise the delay value
    if (delay <= 0)
        return DEVICE_INVALID_PARAMETER;

    // If the display is free, we can display.
    if (animationMode == ANIMATION_MODE_NONE || animationMode == ANIMATION_MODE_STOPPED)
    {
        // Assume right to left functionality, to align with scrollString()
        stride = -stride;

        // calculate starting position which is offset by the stride
        scrollingImagePosition = (startingPosition == DISPLAY_ANIMATE_DEFAULT_POS) ? getWidth() + stride : startingPosition;
        scrollingImageStride = stride;
        scrollingImage = image;
        scrollingImageRendered = false;

        animationDelay = stride == 0 ? 0 : delay;
        animationTick = delay - 1;
        animationMode = autoClear ? ANIMATION_MODE_ANIMATE_IMAGE_WITH_CLEAR : ANIMATION_MODE_ANIMATE_IMAGE;
    }
    else
    {
        return DEVICE_BUSY;
    }

    return DEVICE_OK;
}

int MicroBitDisplay::animate(Image image, int delay, int stride, int startingPosition, int autoClear)
{
    // sanitise the delay value
    if (delay <= 0)
        return DEVICE_INVALID_PARAMETER;

    // If there's an ongoing animation, wait for our turn to display.
    this->waitForFreeDisplay();

    // If the display is free, it's our turn to display.
    // If someone called stopAnimation(), then we simply skip...
    if (animationMode == ANIMATION_MODE_NONE)
    {
        // Start the effect.
        this->animateAsync(image, delay, stride, startingPosition, autoClear);

        // Wait for completion.
        fiberWait();
    }
    else
    {
        return DEVICE_CANCELLED;
    }

    return DEVICE_OK;
}
