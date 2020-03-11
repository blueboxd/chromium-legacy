// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_AW_ORIGIN_MATCHER_MOJOM_TRAITS_H_
#define ANDROID_WEBVIEW_COMMON_AW_ORIGIN_MATCHER_MOJOM_TRAITS_H_

#include <string>
#include <vector>

#include "android_webview/common/aw_origin_matcher.h"
#include "android_webview/common/aw_origin_matcher.mojom.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<android_webview::mojom::AwOriginMatcherDataView,
                    android_webview::AwOriginMatcher> {
 public:
  static std::vector<std::string> rules(
      const android_webview::AwOriginMatcher& r) {
    return r.Serialize();
  }

  static bool Read(android_webview::mojom::AwOriginMatcherDataView data,
                   android_webview::AwOriginMatcher* out) {
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

#endif  // ANDROID_WEBVIEW_COMMON_AW_ORIGIN_MATCHER_MOJOM_TRAITS_H_
