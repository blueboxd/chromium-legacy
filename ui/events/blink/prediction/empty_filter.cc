// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/prediction/empty_filter.h"

#include "third_party/blink/public/common/features.h"

namespace ui {

EmptyFilter::EmptyFilter() {}
EmptyFilter::~EmptyFilter() {}

bool EmptyFilter::Filter(const base::TimeTicks& timestamp,
                         gfx::PointF* position) const {
  return position != nullptr;
}

const char* EmptyFilter::GetName() const {
  return blink::features::kFilterNameEmpty;
}

InputFilter* EmptyFilter::Clone() {
  return new EmptyFilter();
}

void EmptyFilter::Reset() {}

}  // namespace ui
