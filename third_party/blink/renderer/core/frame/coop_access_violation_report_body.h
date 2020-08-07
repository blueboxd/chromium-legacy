// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_COOP_ACCESS_VIOLATION_REPORT_BODY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_COOP_ACCESS_VIOLATION_REPORT_BODY_H_

#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/frame/location_report_body.h"

namespace blink {

class CORE_EXPORT CoopAccessViolationReportBody : public LocationReportBody {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CoopAccessViolationReportBody(std::unique_ptr<SourceLocation> source_location,
                                const String& property);
  ~CoopAccessViolationReportBody() final = default;
  const String& property() const { return property_; }
  void BuildJSONValue(V8ObjectBuilder& builder) const final;

 private:
  const String property_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_COOP_ACCESS_VIOLATION_REPORT_BODY_H_
