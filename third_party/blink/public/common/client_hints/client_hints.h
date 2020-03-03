// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_CLIENT_HINTS_CLIENT_HINTS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_CLIENT_HINTS_CLIENT_HINTS_H_

#include <stddef.h>
#include <string>

#include "base/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/web_client_hints/web_client_hints_types.mojom-shared.h"

namespace blink {

// Mapping from WebClientHintsType to the hint's name in header values (e.g.
// kLang => "lang"), and to the hint's outgoing header (e.g. kLang =>
// "sec-ch-lang"). The ordering matches the ordering of enums in
// third_party/blink/public/platform/web_client_hints_type.h.
BLINK_COMMON_EXPORT extern const char* const kClientHintsNameMapping[];
BLINK_COMMON_EXPORT extern const char* const kClientHintsHeaderMapping[];
BLINK_COMMON_EXPORT extern const size_t kClientHintsMappingsCount;

// Mapping from WebEffectiveConnectionType to the header value. This value is
// sent to the origins and is returned by the JavaScript API. The ordering
// should match the ordering in //net/nqe/effective_connection_type.h and
// public/platform/WebEffectiveConnectionType.h.
// This array should be updated if either of the enums in
// effective_connection_type.h or WebEffectiveConnectionType.h are updated.
BLINK_COMMON_EXPORT extern const char* const
    kWebEffectiveConnectionTypeMapping[];

BLINK_COMMON_EXPORT extern const size_t kWebEffectiveConnectionTypeMappingCount;

// Given a comma-separated, ordered list of language codes, return the list
// formatted as a structured header, as described in
// https://tools.ietf.org/html/draft-west-lang-client-hint-00#section-2.1
std::string BLINK_COMMON_EXPORT
SerializeLangClientHint(const std::string& raw_language_list);

// Tries to parse an Accept-CH header. Returns base::nullopt if parsing
// failed and the header should be ignored; otherwise returns a (possibly
// empty) list of hints to accept.
//
// Language hints will only be in the result if |permit_lang_hints| is true;
// UA-related ones if |permit_ua_hints| is.
base::Optional<std::vector<blink::mojom::WebClientHintsType>>
    BLINK_COMMON_EXPORT ParseAcceptCH(const std::string& header,
                                      bool permit_lang_hints,
                                      bool permit_ua_hints);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_CLIENT_HINTS_CLIENT_HINTS_H_
