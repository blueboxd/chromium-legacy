// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/media_element_parser_helpers.h"

#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

namespace media_element_parser_helpers {

void ReportUnsizedMediaViolation(const LayoutObject* layout_object,
                                 bool send_report) {
  const ComputedStyle& style = layout_object->StyleRef();
  if (!style.LogicalWidth().IsSpecified() &&
      !style.LogicalHeight().IsSpecified()) {
    layout_object->GetDocument().CountPotentialFeaturePolicyViolation(
        mojom::blink::FeaturePolicyFeature::kUnsizedMedia);
    if (send_report) {
      layout_object->GetDocument().ReportFeaturePolicyViolation(
          mojom::blink::FeaturePolicyFeature::kUnsizedMedia,
          mojom::FeaturePolicyDisposition::kEnforce);
    }
  }
}

}  // namespace media_element_parser_helpers

}  // namespace blink
