// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/text_utils.h"

#include <stdint.h>

#include "base/i18n/char_iterator.h"
#include "base/i18n/rtl.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/utf16.h"

namespace gfx {

using base::i18n::UTF16CharIterator;

namespace {

// Returns true if the specified character must be elided from a string.
// Examples are combining marks and whitespace.
bool IsCombiningMark(UChar32 c) {
  const int8_t char_type = u_charType(c);
  return char_type == U_NON_SPACING_MARK || char_type == U_ENCLOSING_MARK ||
         char_type == U_COMBINING_SPACING_MARK;
}

bool IsSpace(UChar32 c) {
  // Ignore NUL character.
  if (!c)
    return false;
  const int8_t char_type = u_charType(c);
  return char_type == U_SPACE_SEPARATOR || char_type == U_LINE_SEPARATOR ||
         char_type == U_PARAGRAPH_SEPARATOR || char_type == U_CONTROL_CHAR;
}

}  // namespace

base::string16 RemoveAcceleratorChar(const base::string16& s,
                                     base::char16 accelerator_char,
                                     int* accelerated_char_pos,
                                     int* accelerated_char_span) {
  bool escaped = false;
  ptrdiff_t last_char_pos = -1;
  int last_char_span = 0;
  UTF16CharIterator chars(&s);
  base::string16 accelerator_removed;

  accelerator_removed.reserve(s.size());
  while (!chars.end()) {
    int32_t c = chars.get();
    int array_pos = chars.array_pos();
    chars.Advance();

    if (c != accelerator_char || escaped) {
      int span = chars.array_pos() - array_pos;
      if (escaped && c != accelerator_char) {
        last_char_pos = accelerator_removed.size();
        last_char_span = span;
      }
      for (int i = 0; i < span; i++)
        accelerator_removed.push_back(s[array_pos + i]);
      escaped = false;
    } else {
      escaped = true;
    }
  }

  if (accelerated_char_pos)
    *accelerated_char_pos = last_char_pos;
  if (accelerated_char_span)
    *accelerated_char_span = last_char_span;

  return accelerator_removed;
}

size_t FindValidBoundaryBefore(const base::string16& text,
                               size_t index,
                               bool trim_whitespace) {
  UTF16CharIterator it = UTF16CharIterator::LowerBound(&text, index);

  // First, move left until we're positioned on a code point that is not a
  // combining mark.
  while (!it.start() && IsCombiningMark(it.get()))
    it.Rewind();

  // Next, maybe trim whitespace to the left of the current position.
  if (trim_whitespace) {
    while (!it.start() && IsSpace(it.PreviousCodePoint()))
      it.Rewind();
  }

  return it.array_pos();
}

size_t FindValidBoundaryAfter(const base::string16& text,
                              size_t index,
                              bool trim_whitespace) {
  UTF16CharIterator it = UTF16CharIterator::UpperBound(&text, index);

  // First, move right until we're positioned on a code point that is not a
  // combining mark.
  while (!it.end() && IsCombiningMark(it.get()))
    it.Advance();

  // Next, maybe trim space at the current position.
  if (trim_whitespace) {
    // A mark combining with a space is renderable, so we'll prevent
    // trimming spaces with combining marks.
    while (!it.end() && IsSpace(it.get()) &&
           !IsCombiningMark(it.NextCodePoint())) {
      it.Advance();
    }
  }

  return it.array_pos();
}

HorizontalAlignment MaybeFlipForRTL(HorizontalAlignment alignment) {
  if (base::i18n::IsRTL() &&
      (alignment == gfx::ALIGN_LEFT || alignment == gfx::ALIGN_RIGHT)) {
    alignment =
        (alignment == gfx::ALIGN_LEFT) ? gfx::ALIGN_RIGHT : gfx::ALIGN_LEFT;
  }
  return alignment;
}

}  // namespace gfx
