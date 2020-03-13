// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_GEOMETRY_CONVERSIONS_H_
#define PDF_GEOMETRY_CONVERSIONS_H_

struct PP_Rect;
struct PP_Size;

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace chrome_pdf {

gfx::Rect RectFromPPRect(const PP_Rect& pp_rect);
gfx::Size SizeFromPPSize(const PP_Size& pp_size);

}  // namespace chrome_pdf

#endif  // PDF_GEOMETRY_CONVERSIONS_H_
