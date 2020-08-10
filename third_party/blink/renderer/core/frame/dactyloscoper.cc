// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/dactyloscoper.h"

#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"

namespace blink {

Dactyloscoper::Dactyloscoper() = default;

void Dactyloscoper::Record(WebFeature feature) {
  // TODO(mkwst): This is a stub. We'll pull in more interesting functionality
  // here over time.
}

// static
void Dactyloscoper::Record(ExecutionContext* context, WebFeature feature) {
  // TODO: Workers.
  if (!context)
    return;
  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    if (auto* frame = window->GetFrame())
      frame->Loader().GetDocumentLoader()->GetDactyloscoper().Record(feature);
  }
}

// static
void Dactyloscoper::RecordDirectSurface(ExecutionContext* context,
                                        WebFeature feature,
                                        IdentifiableToken value) {
  if (!IdentifiabilityStudySettings::Get()->IsActive())
    return;
  auto* window = DynamicTo<LocalDOMWindow>(context);
  if (!window)
    return;
  Document* document = window->document();
  IdentifiabilityMetricBuilder(document->UkmSourceID())
      .SetWebfeature(feature, value)
      .Record(document->UkmRecorder());
}

void Dactyloscoper::RecordDirectSurface(ExecutionContext* context,
                                        WebFeature feature,
                                        String str) {
  if (!IdentifiabilityStudySettings::Get()->IsActive())
    return;
  if (str.IsEmpty())
    return;
  Dactyloscoper::RecordDirectSurface(context, feature,
                                     IdentifiabilitySensitiveStringToken(str));
}

void Dactyloscoper::RecordDirectSurface(ExecutionContext* context,
                                        WebFeature feature,
                                        Vector<String> strs) {
  if (!IdentifiabilityStudySettings::Get()->IsActive())
    return;
  if (strs.IsEmpty())
    return;
  IdentifiableTokenBuilder builder;
  for (const auto& str : strs) {
    builder.AddToken(IdentifiabilitySensitiveStringToken(str));
  }
  Dactyloscoper::RecordDirectSurface(context, feature, builder.GetToken());
}

}  // namespace blink
