// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_hit_test_source.h"

#include "third_party/blink/renderer/modules/xr/xr_hit_test_options.h"
#include "third_party/blink/renderer/modules/xr/xr_hit_test_result.h"

namespace blink {

XRHitTestSource::XRHitTestSource(uint32_t id, XRHitTestOptions* options)
    : id_(id), options_(options) {}

uint32_t XRHitTestSource::id() const {
  return id_;
}

XRHitTestOptions* XRHitTestSource::hitTestOptions() const {
  return options_;
}

HeapVector<Member<XRHitTestResult>> XRHitTestSource::Results() {
  HeapVector<Member<XRHitTestResult>> results;

  for (const auto& result : last_frame_results_) {
    results.emplace_back(MakeGarbageCollected<XRHitTestResult>(this, *result));
  }

  return results;
}

void XRHitTestSource::Update(
    const WTF::Vector<device::mojom::blink::XRHitResultPtr>& hit_test_results) {
  last_frame_results_.clear();

  for (auto& result : hit_test_results) {
    last_frame_results_.push_back(
        std::make_unique<TransformationMatrix>(result->hit_matrix.matrix()));
  }
}

void XRHitTestSource::Trace(blink::Visitor* visitor) {
  visitor->Trace(options_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
