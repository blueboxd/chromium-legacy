// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_EARLY_BREAK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_EARLY_BREAK_H_

namespace blink {

// Possible early unforced breakpoint. This represents a possible (and good)
// location to break. In cases where we run out of space at an unideal location,
// we may want to go back and break here instead.
class NGEarlyBreak {
 public:
  explicit NGEarlyBreak(int line_number) : line_number_(line_number) {}

  int LineNumber() const { return line_number_; }

 private:
  int line_number_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_EARLY_BREAK_H_
