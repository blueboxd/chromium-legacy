// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_outside_list_marker.h"

namespace blink {

LayoutOutsideListMarker::LayoutOutsideListMarker(Element* element)
    : LayoutListMarker(element) {}

LayoutOutsideListMarker::~LayoutOutsideListMarker() = default;

}  // namespace blink
