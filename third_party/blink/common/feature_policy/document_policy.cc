// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/feature_policy/document_policy.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "net/http/structured_headers.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom.h"

namespace blink {

// static
std::unique_ptr<DocumentPolicy> DocumentPolicy::CreateWithHeaderPolicy(
    const ParsedDocumentPolicy& header_policy) {
  DocumentPolicy::FeatureState feature_defaults;
  for (const auto& entry : GetDocumentPolicyFeatureInfoMap())
    feature_defaults.emplace(entry.first, entry.second.default_value);
  return CreateWithHeaderPolicy(header_policy.feature_state,
                                header_policy.endpoint_map, feature_defaults);
}

namespace {
net::structured_headers::Item PolicyValueToItem(const PolicyValue& value) {
  switch (value.Type()) {
    case mojom::PolicyValueType::kBool:
      return net::structured_headers::Item{value.BoolValue()};
    case mojom::PolicyValueType::kDecDouble:
      return net::structured_headers::Item{value.DoubleValue()};
    default:
      NOTREACHED();
      return net::structured_headers::Item{
          nullptr, net::structured_headers::Item::ItemType::kNullType};
  }
}

}  // namespace

// static
base::Optional<std::string> DocumentPolicy::Serialize(
    const FeatureState& policy) {
  return DocumentPolicy::SerializeInternal(policy,
                                           GetDocumentPolicyFeatureInfoMap());
}

// static
base::Optional<std::string> DocumentPolicy::SerializeInternal(
    const FeatureState& policy,
    const DocumentPolicyFeatureInfoMap& feature_info_map) {
  net::structured_headers::List root;
  root.reserve(policy.size());

  std::vector<std::pair<mojom::DocumentPolicyFeature, PolicyValue>>
      sorted_policy(policy.begin(), policy.end());
  std::sort(sorted_policy.begin(), sorted_policy.end(),
            [&](const auto& a, const auto& b) {
              const std::string& feature_a =
                  feature_info_map.at(a.first).feature_name;
              const std::string& feature_b =
                  feature_info_map.at(b.first).feature_name;
              return feature_a < feature_b;
            });

  for (const auto& policy_entry : sorted_policy) {
    const auto& info = feature_info_map.at(policy_entry.first /* feature */);

    const PolicyValue& value = policy_entry.second;
    if (value.Type() == mojom::PolicyValueType::kBool) {
      root.push_back(net::structured_headers::ParameterizedMember(
          net::structured_headers::Item(
              (value.BoolValue() ? "" : "no-") + info.feature_name,
              net::structured_headers::Item::ItemType::kTokenType),
          {}));
    } else {
      net::structured_headers::Parameters params;
      params.push_back(std::pair<std::string, net::structured_headers::Item>{
          info.feature_param_name, PolicyValueToItem(value)});
      root.push_back(net::structured_headers::ParameterizedMember(
          net::structured_headers::Item(
              info.feature_name,
              net::structured_headers::Item::ItemType::kTokenType),
          params));
    }
  }

  return net::structured_headers::SerializeList(root);
}

// static
DocumentPolicy::FeatureState DocumentPolicy::MergeFeatureState(
    const DocumentPolicy::FeatureState& policy1,
    const DocumentPolicy::FeatureState& policy2) {
  DocumentPolicy::FeatureState result;
  auto i1 = policy1.begin();
  auto i2 = policy2.begin();

  // Because std::map is by default ordered in ascending order based on key
  // value, we can run 2 iterators simultaneously through both maps to merge
  // them.
  while (i1 != policy1.end() || i2 != policy2.end()) {
    if (i1 == policy1.end()) {
      result.insert(*i2);
      i2++;
    } else if (i2 == policy2.end()) {
      result.insert(*i1);
      i1++;
    } else {
      if (i1->first == i2->first) {
        // Take the stricter policy when there is a key conflict.
        result.emplace(i1->first, std::min(i1->second, i2->second));
        i1++;
        i2++;
      } else if (i1->first < i2->first) {
        result.insert(*i1);
        i1++;
      } else {
        result.insert(*i2);
        i2++;
      }
    }
  }

  return result;
}

bool DocumentPolicy::IsFeatureEnabled(
    mojom::DocumentPolicyFeature feature) const {
  mojom::PolicyValueType feature_type =
      GetDocumentPolicyFeatureInfoMap().at(feature).default_value.Type();
  return IsFeatureEnabled(feature,
                          PolicyValue::CreateMaxPolicyValue(feature_type));
}

bool DocumentPolicy::IsFeatureEnabled(
    mojom::DocumentPolicyFeature feature,
    const PolicyValue& threshold_value) const {
  return GetFeatureValue(feature) >= threshold_value;
}

PolicyValue DocumentPolicy::GetFeatureValue(
    mojom::DocumentPolicyFeature feature) const {
  return internal_feature_state_[static_cast<size_t>(feature)];
}

const base::Optional<std::string> DocumentPolicy::GetFeatureEndpoint(
    mojom::DocumentPolicyFeature feature) const {
  auto endpoint_it = endpoint_map_.find(feature);
  if (endpoint_it != endpoint_map_.end()) {
    return endpoint_it->second;
  } else {
    return base::nullopt;
  }
}

bool DocumentPolicy::IsFeatureSupported(
    mojom::DocumentPolicyFeature feature) const {
  // TODO(iclelland): Generate this switch block
  switch (feature) {
    case mojom::DocumentPolicyFeature::kFontDisplay:
    case mojom::DocumentPolicyFeature::kUnoptimizedLosslessImages:
    case mojom::DocumentPolicyFeature::kForceLoadAtTop:
      return true;
    default:
      return false;
  }
}

void DocumentPolicy::UpdateFeatureState(const FeatureState& feature_state) {
  for (const auto& feature_and_value : feature_state) {
    internal_feature_state_[static_cast<size_t>(feature_and_value.first)] =
        feature_and_value.second;
  }
}

DocumentPolicy::DocumentPolicy(const FeatureState& header_policy,
                               const FeatureEndpointMap& endpoint_map,
                               const DocumentPolicy::FeatureState& defaults)
    : endpoint_map_(endpoint_map) {
  // Fill the internal feature state with default value first,
  // and overwrite the value if it is specified in the header.
  UpdateFeatureState(defaults);
  UpdateFeatureState(header_policy);
}

// static
std::unique_ptr<DocumentPolicy> DocumentPolicy::CreateWithHeaderPolicy(
    const FeatureState& header_policy,
    const FeatureEndpointMap& endpoint_map,
    const DocumentPolicy::FeatureState& defaults) {
  std::unique_ptr<DocumentPolicy> new_policy = base::WrapUnique(
      new DocumentPolicy(header_policy, endpoint_map, defaults));
  return new_policy;
}

// static
bool DocumentPolicy::IsPolicyCompatible(
    const DocumentPolicy::FeatureState& required_policy,
    const DocumentPolicy::FeatureState& incoming_policy) {
  for (const auto& required_entry : required_policy) {
    // feature value > threshold => enabled, where feature value is the value in
    // document policy and threshold is the value to test against.
    // The smaller the feature value, the stricter the policy.
    // Incoming policy should be at least as strict as the required one.
    const auto& feature = required_entry.first;
    const auto& required_value = required_entry.second;
    // Use default value when incoming policy does not specify a value.
    const auto incoming_entry = incoming_policy.find(feature);
    const auto& incoming_value =
        incoming_entry != incoming_policy.end()
            ? incoming_entry->second
            : GetDocumentPolicyFeatureInfoMap().at(feature).default_value;

    if (required_value < incoming_value)
      return false;
  }
  return true;
}

}  // namespace blink
