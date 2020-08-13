// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/plugins/navigator_plugins.h"

#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/modules/plugins/dom_mime_type_array.h"
#include "third_party/blink/renderer/modules/plugins/dom_plugin_array.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"

namespace blink {

NavigatorPlugins::NavigatorPlugins(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

// static
NavigatorPlugins& NavigatorPlugins::From(Navigator& navigator) {
  NavigatorPlugins* supplement = ToNavigatorPlugins(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorPlugins>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

// static
NavigatorPlugins* NavigatorPlugins::ToNavigatorPlugins(Navigator& navigator) {
  return Supplement<Navigator>::From<NavigatorPlugins>(navigator);
}

// static
const char NavigatorPlugins::kSupplementName[] = "NavigatorPlugins";

// static
DOMPluginArray* NavigatorPlugins::plugins(Navigator& navigator) {
  return NavigatorPlugins::From(navigator).plugins(navigator.GetFrame());
}

// static
DOMMimeTypeArray* NavigatorPlugins::mimeTypes(Navigator& navigator) {
  return NavigatorPlugins::From(navigator).mimeTypes(navigator.GetFrame());
}

// static
bool NavigatorPlugins::javaEnabled(Navigator& navigator) {
  return false;
}

DOMPluginArray* NavigatorPlugins::plugins(LocalFrame* frame) const {
  if (!plugins_)
    plugins_ = MakeGarbageCollected<DOMPluginArray>(frame);

  DOMPluginArray* result = plugins_.Get();
  if (!IdentifiabilityStudySettings::Get()->IsActive() || !frame)
    return result;
  Document* document = frame->GetDocument();
  if (!document)
    return result;

  // Build digest...
  IdentifiableTokenBuilder builder;
  for (unsigned i = 0; i < result->length(); i++) {
    DOMPlugin* plugin = result->item(i);
    builder.AddToken(IdentifiabilityBenignStringToken(plugin->name()))
        .AddToken(IdentifiabilityBenignStringToken(plugin->description()))
        .AddToken(IdentifiabilityBenignStringToken(plugin->filename()));
    for (unsigned j = 0; j < plugin->length(); j++) {
      DOMMimeType* mimeType = plugin->item(j);
      builder.AddToken(IdentifiabilityBenignStringToken(mimeType->type()))
          .AddToken(IdentifiabilityBenignStringToken(mimeType->description()))
          .AddToken(IdentifiabilityBenignStringToken(mimeType->suffixes()));
    }
  }
  // ...and report to UKM.
  IdentifiabilityMetricBuilder(document->UkmSourceID())
      .SetWebfeature(WebFeature::kNavigatorPlugins, builder.GetToken())
      .Record(document->UkmRecorder());

  return result;
}

DOMMimeTypeArray* NavigatorPlugins::mimeTypes(LocalFrame* frame) const {
  if (!mime_types_)
    mime_types_ = MakeGarbageCollected<DOMMimeTypeArray>(frame);
  return mime_types_.Get();
}

void NavigatorPlugins::Trace(Visitor* visitor) const {
  visitor->Trace(plugins_);
  visitor->Trace(mime_types_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
