// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_COMMON_AW_ORIGIN_MATCHER_MOJOM_TRAITS_H_
#define COMPONENTS_JS_INJECTION_COMMON_AW_ORIGIN_MATCHER_MOJOM_TRAITS_H_

#include <string>
#include <vector>

#include "components/js_injection/common/aw_origin_matcher.h"
#include "components/js_injection/common/aw_origin_matcher.mojom.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<js_injection::mojom::AwOriginMatcherDataView,
                    js_injection::AwOriginMatcher> {
 public:
  static std::vector<std::string> rules(
      const js_injection::AwOriginMatcher& r) {
    return r.Serialize();
  }

  static bool Read(js_injection::mojom::AwOriginMatcherDataView data,
                   js_injection::AwOriginMatcher* out) {
    std::vector<std::string> rules;
    if (!data.ReadRules(&rules))
      return false;
    for (const auto& rule : rules) {
      if (!out->AddRuleFromString(rule))
        return false;
    }
    return true;
  }
};

}  // namespace mojo

#endif  // COMPONENTS_JS_INJECTION_COMMON_AW_ORIGIN_MATCHER_MOJOM_TRAITS_H_
