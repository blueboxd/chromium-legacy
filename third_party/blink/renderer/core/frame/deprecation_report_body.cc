// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/deprecation_report_body.h"

#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/text/date_components.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

ScriptValue DeprecationReportBody::anticipatedRemoval(
    ScriptState* script_state) const {
  v8::Isolate* isolate = script_state->GetIsolate();
  // We can't use AnticipatedRemoval() here because it's time value is not
  // compatible due to FromDoubleT().
  if (!anticipatedRemoval_)
    return ScriptValue::CreateNull(isolate);
  return ScriptValue(
      isolate, ToV8(base::Time::FromJsTime(anticipatedRemoval_), script_state));
}

base::Optional<base::Time> DeprecationReportBody::AnticipatedRemoval() const {
  if (!anticipatedRemoval_)
    return base::nullopt;
  return base::Time::FromDoubleT(anticipatedRemoval_);
}

void DeprecationReportBody::BuildJSONValue(V8ObjectBuilder& builder) const {
  LocationReportBody::BuildJSONValue(builder);
  builder.AddString("id", id());
  builder.AddString("message", message());

  if (!anticipatedRemoval_) {
    builder.AddNull("anticipatedRemoval");
  } else {
    DateComponents anticipated_removal_date;
    bool is_valid =
        anticipated_removal_date.SetMillisecondsSinceEpochForDateTimeLocal(
            anticipatedRemoval_);
    if (!is_valid) {
      builder.AddNull("anticipatedRemoval");
    } else {
      // Adding extra 'Z' here to ensure that the string gives the same result
      // as JSON.stringify(anticipatedRemoval) in javascript. Note here
      // anticipatedRemoval will become a Date object in javascript.
      String iso8601_date =
          anticipated_removal_date.ToString(DateComponents::kMillisecond) + "Z";
      builder.AddString("anticipatedRemoval", iso8601_date);
    }
  }
}

}  // namespace blink
