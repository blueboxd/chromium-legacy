// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/rectf.h"

namespace autofill_assistant {

RectF::RectF() : left(0.0f), top(0.0f), right(0.0f), bottom(0.0f) {}
RectF::RectF(float l, float t, float r, float b)
    : left(l), top(t), right(r), bottom(b) {}

bool RectF::empty() const {
  return right <= left || bottom <= top;
}

bool RectF::operator==(const RectF& another) const {
  return left == another.left && top == another.top && right == another.right &&
         bottom == another.bottom;
}

std::ostream& operator<<(std::ostream& out, const RectF& rect) {
  out << "[l: " << rect.left << ", t: " << rect.top << ", r: " << rect.right
      << ", b: " << rect.bottom << "]";
  return out;
}

}  // namespace autofill_assistant
