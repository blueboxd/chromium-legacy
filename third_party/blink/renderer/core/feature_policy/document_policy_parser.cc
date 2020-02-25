// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/feature_policy/document_policy_parser.h"

#include "net/http/structured_headers.h"
#include "third_party/blink/public/mojom/feature_policy/policy_value.mojom-blink-forward.h"

namespace blink {

base::Optional<PolicyValue> ItemToPolicyValue(
    const net::structured_headers::Item& item) {
  switch (item.Type()) {
    case net::structured_headers::Item::ItemType::kIntegerType:
      return PolicyValue(static_cast<double>(item.GetInteger()));
    case net::structured_headers::Item::ItemType::kFloatType:
      return PolicyValue(item.GetFloat());
    default:
      return base::nullopt;
  }
}

// static
base::Optional<DocumentPolicy::FeatureState> DocumentPolicyParser::Parse(
    const String& policy_string) {
  return ParseInternal(policy_string, GetDocumentPolicyNameFeatureMap(),
                       GetDocumentPolicyFeatureInfoMap(),
                       GetAvailableDocumentPolicyFeatures());
}

// static
base::Optional<DocumentPolicy::FeatureState>
DocumentPolicyParser::ParseInternal(
    const String& policy_string,
    const DocumentPolicyNameFeatureMap& name_feature_map,
    const DocumentPolicyFeatureInfoMap& feature_info_map,
    const DocumentPolicyFeatureSet& available_features) {
  auto root = net::structured_headers::ParseList(policy_string.Ascii());
  if (!root)
    return base::nullopt;

  DocumentPolicy::FeatureState policy;
  for (const net::structured_headers::ParameterizedMember& directive :
       root.value()) {
    // Directives must not be inner lists.
    if (directive.member_is_inner_list)
      return base::nullopt;

    const net::structured_headers::Item& feature_token =
        directive.member.front().item;

    // The item in directive should be token type.
    if (!feature_token.is_token())
      return base::nullopt;

    // Feature policy now only support boolean and double PolicyValue
    // which correspond to 0 and 1 param number.
    if (directive.params.size() > 1)
      return base::nullopt;

    base::Optional<PolicyValue> policy_value;
    std::string feature_name = feature_token.GetString();

    if (directive.params.empty()) {  // boolean value
      // handle "no-" prefix
      const std::string& feature_str = feature_token.GetString();
      const bool bool_val =
          feature_str.size() < 3 || feature_str.substr(0, 3) != "no-";
      policy_value = PolicyValue(bool_val);
      if (!bool_val) {  // drop "no-" prefix
        feature_name = feature_name.substr(3);
      }
    } else {  // double value
      policy_value =
          ItemToPolicyValue(directive.params.front().second /* param value */);
    }

    if (!policy_value)
      return base::nullopt;

    if (name_feature_map.find(feature_name) ==
        name_feature_map.end())  // Unrecognized feature name.
      return base::nullopt;

    const mojom::blink::DocumentPolicyFeature feature =
        name_feature_map.at(feature_name);

    // If feature is not available, i.e. not enabled, ignore the entry.
    if (available_features.find(feature) == available_features.end())
      continue;

    if (feature_info_map.at(feature).default_value.Type() !=
        policy_value->Type())  // Invalid value type.
      return base::nullopt;

    if ((*policy_value).Type() != mojom::blink::PolicyValueType::kBool &&
        feature_info_map.at(feature).feature_param_name !=
            directive.params.front().first)  // Invalid param key name.
      return base::nullopt;

    policy.emplace(feature, std::move(*policy_value));
  }
  return policy;
}

}  // namespace blink
