// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/client_hints/client_hints.h"

#include "base/stl_util.h"
#include "base/strings/string_tokenizer.h"

namespace blink {

const char* const kClientHintsNameMapping[] = {
    "device-memory", "dpr",  "width", "viewport-width", "rtt",      "downlink",
    "ect",           "lang", "ua",    "arch",           "platform", "model",
    "mobile"};

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
};

const size_t kClientHintsMappingsCount = base::size(kClientHintsNameMapping);

static_assert(base::size(kClientHintsNameMapping) ==
                  base::size(kClientHintsHeaderMapping),
              "The Client Hint name and header mappings must contain the same "
              "number of entries.");

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

}  // namespace blink
