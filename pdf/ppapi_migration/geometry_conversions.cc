// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/geometry_conversions.h"

#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chrome_pdf {

gfx::Rect RectFromPPRect(const PP_Rect& pp_rect) {
  return gfx::Rect(pp_rect.point.x, pp_rect.point.y, pp_rect.size.width,
                   pp_rect.size.height);
}

gfx::Size SizeFromPPSize(const PP_Size& pp_size) {
  return gfx::Size(pp_size.width, pp_size.height);
}

gfx::Point PointFromPPPoint(const PP_Point& pp_point) {
  return gfx::Point(pp_point.x, pp_point.y);
}

PP_Point PPPointFromPoint(const gfx::Point& point) {
  return {point.x(), point.y()};
}

}  // namespace chrome_pdf
