// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/activity_tracker_annotation.h"

namespace browser_watcher {

const char ActivityTrackerAnnotation::kAnnotationName[] =
    "ActivityTrackerLocation";

ActivityTrackerAnnotation::ActivityTrackerAnnotation(const void* address,
                                                     size_t size)
    : crashpad::Annotation(kAnnotationType, kAnnotationName, &value_) {
  value_.address = reinterpret_cast<uint64_t>(address);
  value_.size = size;
  SetSize(sizeof(value_));
}

}  // namespace browser_watcher
