// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/size_conversions.h"

#include "base/numerics/safe_conversions.h"

namespace gfx {

Size ToFlooredSize(const SizeF& size) {
  return Size(base::Floor(size.width()), base::Floor(size.height()));
}

Size ToCeiledSize(const SizeF& size) {
  return Size(base::Ceil(size.width()), base::Ceil(size.height()));
}

Size ToRoundedSize(const SizeF& size) {
  return Size(base::Round(size.width()), base::Round(size.height()));
}

}  // namespace gfx

