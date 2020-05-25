// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_PAINT_PREVIEW_COMPOSITOR_SKP_RESULT_H_
#define COMPONENTS_SERVICES_PAINT_PREVIEW_COMPOSITOR_SKP_RESULT_H_

#include "components/paint_preview/common/serial_utils.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace paint_preview {

// Struct for containing an SkPicture and its deserialization context.
struct SkpResult {
  SkpResult();
  ~SkpResult();

  SkpResult(const SkpResult& other) = delete;
  SkpResult& operator=(const SkpResult& rhs) = delete;

  SkpResult(SkpResult&& other);
  SkpResult& operator=(SkpResult&& rhs);

  sk_sp<SkPicture> skp;
  DeserializationContext ctx;
};

}  // namespace paint_preview

#endif  // COMPONENTS_SERVICES_PAINT_PREVIEW_COMPOSITOR_SKP_RESULT_H_
