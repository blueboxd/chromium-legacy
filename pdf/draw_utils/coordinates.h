// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_DRAW_UTILS_COORDINATES_H_
#define PDF_DRAW_UTILS_COORDINATES_H_

#include "ppapi/cpp/rect.h"

namespace pp {
class Point;
}

namespace chrome_pdf {
namespace draw_utils {

// Given |rect| in document coordinates, a |position| in screen coordinates, and
// a |zoom| factor, returns the rectangle in screen coordinates (i.e. 0,0 is top
// left corner of plugin area).
// An empty |rect| will always result in an empty output rect.
// For |zoom|, a value of 1 means 100%. |zoom| is never less than or equal to 0.
pp::Rect GetScreenRect(const pp::Rect& rect,
                       const pp::Point& position,
                       double zoom);

}  // namespace draw_utils
}  // namespace chrome_pdf

#endif  // PDF_DRAW_UTILS_COORDINATES_H_
