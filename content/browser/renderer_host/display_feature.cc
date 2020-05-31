// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/display_feature.h"

namespace content {

bool DisplayFeature::operator==(const DisplayFeature& other) const {
  return orientation == other.orientation && offset == other.offset &&
         mask_length == other.mask_length;
}

bool DisplayFeature::operator!=(const DisplayFeature& other) const {
  return !(*this == other);
}

}  // namespace content
