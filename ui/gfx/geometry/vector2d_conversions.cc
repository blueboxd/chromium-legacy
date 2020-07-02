// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/vector2d_conversions.h"

#include "base/numerics/safe_conversions.h"

namespace gfx {

Vector2d ToFlooredVector2d(const Vector2dF& vector2d) {
  return Vector2d(base::Floor(vector2d.x()), base::Floor(vector2d.y()));
}

Vector2d ToCeiledVector2d(const Vector2dF& vector2d) {
  return Vector2d(base::Ceil(vector2d.x()), base::Ceil(vector2d.y()));
}

Vector2d ToRoundedVector2d(const Vector2dF& vector2d) {
  return Vector2d(base::Round(vector2d.x()), base::Round(vector2d.y()));
}

}  // namespace gfx

