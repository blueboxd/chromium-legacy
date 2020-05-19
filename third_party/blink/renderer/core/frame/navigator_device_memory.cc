// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/navigator_device_memory.h"

#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-shared.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

NavigatorDeviceMemory::NavigatorDeviceMemory(Document* document)
    : document_(document) {}

float NavigatorDeviceMemory::deviceMemory() const {
  float result = ApproximatedDeviceMemory::GetApproximatedDeviceMemory();
  if (document_) {
    IdentifiabilityMetricBuilder(
        base::UkmSourceId::FromInt64(document_->UkmSourceID()))
        .Set(IdentifiableSurface::FromTypeAndInput(
                IdentifiableSurface::Type::kWebFeature,
                static_cast<uint64_t>(WebFeature::kNavigatorDeviceMemory)),
            IdentifiabilityDigestHelper(result))
        .Record(document_->UkmRecorder());
  }
  return result;
}

void NavigatorDeviceMemory::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
}

}  // namespace blink
