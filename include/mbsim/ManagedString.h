// mbsim: desktop stand-in for CODAL's ManagedString.
// API ported from codal-core/inc/types/ManagedString.h
// Copyright (c) 2017 Lancaster University. MIT licence, see LICENSE.
#ifndef MBSIM_MANAGED_STRING_H
#define MBSIM_MANAGED_STRING_H

#include <stdint.h>
#include <memory>
#include <string>

namespace codal
{
    class ManagedString;
    ManagedString operator+(const ManagedString &lhs, const ManagedString &rhs);

    /**
      * An immutable, reference counted string, as in CODAL.
      *
      * The constructors are deliberately not `explicit`, exactly as in CODAL, so that
      * `uBit.display.scroll(42)` converts the int into a string.
      */
    class ManagedString
    {
        std::shared_ptr<const std::string> ptr;

        public:
        ManagedString();
        ManagedString(const char *str);
        ManagedString(const int value);
        ManagedString(const char value);
        ManagedString(const char *str, const int16_t length);
        ManagedString(const ManagedString &s);
        ManagedString(const ManagedString &s1, const ManagedString &s2);
        ~ManagedString();

        ManagedString &operator=(const ManagedString &s);
        bool operator==(const ManagedString &s);
        bool operator!=(const ManagedString &s);
        bool operator<(const ManagedString &s);
        bool operator>(const ManagedString &s);

        ManagedString substring(int16_t start, int16_t length);
        friend ManagedString(codal::operator+)(const ManagedString &lhs, const ManagedString &rhs);
        char charAt(int16_t index);

        const char *toCharArray() const { return ptr->c_str(); }
        int16_t length() const { return (int16_t)ptr->size(); }

        static ManagedString EmptyString;
    };
}

#endif
