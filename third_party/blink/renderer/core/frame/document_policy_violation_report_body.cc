// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/document_policy_violation_report_body.h"

namespace blink {

void DocumentPolicyViolationReportBody::BuildJSONValue(
    V8ObjectBuilder& builder) const {
  LocationReportBody::BuildJSONValue(builder);
  builder.AddString("featureId", featureId());
  builder.AddString("disposition", disposition());
  builder.AddStringOrNull("message", message());
}

}  // namespace blink
