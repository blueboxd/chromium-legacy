// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/client_hints/client_hints.h"

#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "net/http/structured_headers.h"

namespace blink {

const char* const kClientHintsNameMapping[] = {"device-memory",
                                               "dpr",
                                               "width",
                                               "viewport-width",
                                               "rtt",
                                               "downlink",
                                               "ect",
                                               "lang",
                                               "ua",
                                               "ua-arch",
                                               "ua-platform",
                                               "ua-model",
                                               "ua-mobile",
                                               "ua-full-version"};

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
};

const size_t kClientHintsMappingsCount = base::size(kClientHintsNameMapping);

static_assert(base::size(kClientHintsNameMapping) ==
                  base::size(kClientHintsHeaderMapping),
              "The Client Hint name and header mappings must contain the same "
              "number of entries.");

static_assert(
    base::size(kClientHintsNameMapping) ==
        (static_cast<int>(mojom::WebClientHintsType::kMaxValue) + 1),
    "Client Hint name table size must match mojom::WebClientHintsType range");

const char* const kWebEffectiveConnectionTypeMapping[] = {
    "4g" /* Unknown */, "4g" /* Offline */, "slow-2g" /* Slow 2G */,
    "2g" /* 2G */,      "3g" /* 3G */,      "4g" /* 4G */
};

const size_t kWebEffectiveConnectionTypeMappingCount =
    base::size(kWebEffectiveConnectionTypeMapping);

namespace {

struct ClientHintNameCompator {
  bool operator()(const std::string& lhs, const std::string& rhs) const {
    return base::CompareCaseInsensitiveASCII(lhs, rhs) < 0;
  }
};

using DecodeMap = base::
    flat_map<std::string, mojom::WebClientHintsType, ClientHintNameCompator>;

DecodeMap MakeDecodeMap() {
  DecodeMap result;
  for (size_t i = 0;
       i < static_cast<int>(mojom::WebClientHintsType::kMaxValue) + 1; ++i) {
    result.insert(std::make_pair(kClientHintsNameMapping[i],
                                 static_cast<mojom::WebClientHintsType>(i)));
  }
  return result;
}

const DecodeMap& GetDecodeMap() {
  static const base::NoDestructor<DecodeMap> decode_map(MakeDecodeMap());
  return *decode_map;
}

}  // namespace

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

base::Optional<std::vector<blink::mojom::WebClientHintsType>> ParseAcceptCH(
    const std::string& header,
    bool permit_lang_hints,
    bool permit_ua_hints) {
  // Accept-CH is an sh-list of tokens; see:
  // https://httpwg.org/http-extensions/client-hints.html#rfc.section.3.1
  base::Optional<net::structured_headers::List> maybe_list =
      net::structured_headers::ParseList(header);
  if (!maybe_list.has_value())
    return base::nullopt;

  // Standard validation rules: we want a list of tokens, so this better
  // only have tokens (but params are OK!)
  for (const auto& list_item : maybe_list.value()) {
    // Make sure not a nested list.
    if (list_item.member.size() != 1u)
      return base::nullopt;
    if (!list_item.member[0].item.is_token())
      return base::nullopt;
  }

  std::vector<blink::mojom::WebClientHintsType> result;

  // Now convert those to actual hint enums.
  const DecodeMap& decode_map = GetDecodeMap();
  for (const auto& list_item : maybe_list.value()) {
    const std::string& token_value = list_item.member[0].item.GetString();

    auto iter = decode_map.find(token_value);
    if (iter != decode_map.end()) {
      mojom::WebClientHintsType hint = iter->second;

      // Some hints are supported only conditionally.
      switch (hint) {
        case mojom::WebClientHintsType::kLang:
          if (permit_lang_hints)
            result.push_back(hint);
          break;
        case mojom::WebClientHintsType::kUA:
        case mojom::WebClientHintsType::kUAArch:
        case mojom::WebClientHintsType::kUAPlatform:
        case mojom::WebClientHintsType::kUAModel:
        case mojom::WebClientHintsType::kUAMobile:
        case mojom::WebClientHintsType::kUAFullVersion:
          if (permit_ua_hints)
            result.push_back(hint);
          break;
        default:
          result.push_back(hint);
      }  // switch (hint)
    }    // if iter != end
  }      // for list_item
  return base::make_optional(std::move(result));
}

}  // namespace blink
