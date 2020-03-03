// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"

#include "base/macros.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

ClientHintsPreferences::ClientHintsPreferences() {
  DCHECK_EQ(static_cast<size_t>(mojom::WebClientHintsType::kMaxValue) + 1,
            kClientHintsMappingsCount);
}

void ClientHintsPreferences::UpdateFrom(
    const ClientHintsPreferences& preferences) {
  for (size_t i = 0;
       i < static_cast<int>(mojom::WebClientHintsType::kMaxValue) + 1; ++i) {
    mojom::WebClientHintsType type = static_cast<mojom::WebClientHintsType>(i);
    enabled_hints_.SetIsEnabled(type, preferences.ShouldSend(type));
  }
}

void ClientHintsPreferences::UpdateFromAcceptClientHintsHeader(
    const String& header_value,
    const KURL& url,
    Context* context) {
  if (header_value.IsEmpty())
    return;

  // Client hints should be allowed only on secure URLs.
  if (!IsClientHintsAllowed(url))
    return;

  // 8-bit conversions from String can turn non-ASCII characters into ?,
  // turning syntax errors into "correct" syntax, so reject those first.
  // (.Utf8() doesn't have this problem, but it does a lot of expensive
  //  work that would be wasted feeding to an ASCII-only syntax).
  if (!header_value.ContainsOnlyASCIIOrEmpty())
    return;

  // Note: .Ascii() would convert tab to ?, which is undesirable.
  base::Optional<std::vector<blink::mojom::WebClientHintsType>> parsed_ch =
      ParseAcceptCH(header_value.Latin1(),
                    RuntimeEnabledFeatures::LangClientHintHeaderEnabled(),
                    RuntimeEnabledFeatures::UserAgentClientHintEnabled());
  if (!parsed_ch.has_value())
    return;

  // Note: this keeps previously enabled hints.
  for (blink::mojom::WebClientHintsType newly_enabled : parsed_ch.value())
    enabled_hints_.SetIsEnabled(newly_enabled, true);

  if (context) {
    for (size_t i = 0;
         i < static_cast<int>(mojom::WebClientHintsType::kMaxValue) + 1; ++i) {
      mojom::WebClientHintsType type =
          static_cast<mojom::WebClientHintsType>(i);
      if (enabled_hints_.IsEnabled(type))
        context->CountClientHints(type);
    }
  }
}

void ClientHintsPreferences::UpdateFromAcceptClientHintsLifetimeHeader(
    const String& header_value,
    const KURL& url,
    Context* context) {
  if (header_value.IsEmpty())
    return;

  // Client hints should be allowed only on secure URLs.
  if (!IsClientHintsAllowed(url))
    return;

  bool conversion_ok = false;
  int64_t persist_duration_seconds = header_value.ToInt64Strict(&conversion_ok);
  if (!conversion_ok || persist_duration_seconds <= 0)
    return;

  persist_duration_ = base::TimeDelta::FromSeconds(persist_duration_seconds);
  if (context)
    context->CountPersistentClientHintHeaders();
}

// static
bool ClientHintsPreferences::IsClientHintsAllowed(const KURL& url) {
  return (url.ProtocolIs("http") || url.ProtocolIs("https")) &&
         (SecurityOrigin::IsSecure(url) ||
          SecurityOrigin::Create(url)->IsLocalhost());
}

WebEnabledClientHints ClientHintsPreferences::GetWebEnabledClientHints() const {
  return enabled_hints_;
}

base::TimeDelta ClientHintsPreferences::GetPersistDuration() const {
  return persist_duration_;
}

}  // namespace blink
