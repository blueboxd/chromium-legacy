// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/client_hints/client_hints.h"

#include <utility>
#include <vector>

#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"

namespace blink {

const char* const kClientHintsHeaderMapping[] = {
    "device-memory",
    "dpr",
    "width",
    "viewport-width",
    "rtt",
    "downlink",
    "ect",
    "sec-ch-lang",
    "sec-ch-ua",
    "sec-ch-ua-arch",
    "sec-ch-ua-platform",
    "sec-ch-ua-model",
    "sec-ch-ua-mobile",
    "sec-ch-ua-full-version",
    "sec-ch-ua-platform-version",
};

const size_t kClientHintsMappingsCount = base::size(kClientHintsHeaderMapping);

static_assert(
    base::size(kClientHintsHeaderMapping) ==
        (static_cast<int>(network::mojom::WebClientHintsType::kMaxValue) + 1),
    "Client Hint name table size must match network::mojom::WebClientHintsType "
    "range");

const char* const kWebEffectiveConnectionTypeMapping[] = {
    "4g" /* Unknown */, "4g" /* Offline */, "slow-2g" /* Slow 2G */,
    "2g" /* 2G */,      "3g" /* 3G */,      "4g" /* 4G */
};

const size_t kWebEffectiveConnectionTypeMappingCount =
    base::size(kWebEffectiveConnectionTypeMapping);

std::string SerializeLangClientHint(const std::string& raw_language_list) {
  base::StringTokenizer t(raw_language_list, ",");
  std::string result;
  while (t.GetNext()) {
    if (!result.empty())
      result.append(", ");

    result.append("\"");
    result.append(t.token().c_str());
    result.append("\"");
  }
  return result;
}

base::Optional<std::vector<network::mojom::WebClientHintsType>> FilterAcceptCH(
    base::Optional<std::vector<network::mojom::WebClientHintsType>> in,
    bool permit_lang_hints,
    bool permit_ua_hints) {
  if (!in.has_value())
    return base::nullopt;

  std::vector<network::mojom::WebClientHintsType> result;
  for (network::mojom::WebClientHintsType hint : in.value()) {
    // Some hints are supported only conditionally.
    switch (hint) {
      case network::mojom::WebClientHintsType::kLang:
        if (permit_lang_hints)
          result.push_back(hint);
        break;
      case network::mojom::WebClientHintsType::kUA:
      case network::mojom::WebClientHintsType::kUAArch:
      case network::mojom::WebClientHintsType::kUAPlatform:
      case network::mojom::WebClientHintsType::kUAPlatformVersion:
      case network::mojom::WebClientHintsType::kUAModel:
      case network::mojom::WebClientHintsType::kUAMobile:
      case network::mojom::WebClientHintsType::kUAFullVersion:
        if (permit_ua_hints)
          result.push_back(hint);
        break;
      default:
        result.push_back(hint);
    }
  }
  return base::make_optional(std::move(result));
}

}  // namespace blink
