// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_IMPRESSION_CONVERSIONS_H_
#define CONTENT_RENDERER_IMPRESSION_CONVERSIONS_H_

#include "content/public/common/impression.h"
#include "third_party/blink/public/platform/web_impression.h"

namespace content {

Impression ConvertWebImpressionToImpression(
    const blink::WebImpression& web_impression);

}  // namespace content

#endif  // CONTENT_RENDERER_IMPRESSION_CONVERSIONS_H_
