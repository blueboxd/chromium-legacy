// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/point_conversions.h"

#include "base/numerics/safe_conversions.h"

namespace gfx {

Point ToFlooredPoint(const PointF& point) {
  return Point(base::Floor(point.x()), base::Floor(point.y()));
}

Point ToCeiledPoint(const PointF& point) {
  return Point(base::Ceil(point.x()), base::Ceil(point.y()));
}

Point ToRoundedPoint(const PointF& point) {
  return Point(base::Round(point.x()), base::Round(point.y()));
}

}  // namespace gfx

