// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/feature_policy/document_policy_parser.h"

#include "net/http/structured_headers.h"
#include "third_party/blink/public/mojom/feature_policy/policy_value.mojom-blink.h"

namespace blink {
namespace {

constexpr const char* kReportTo = "report-to";
constexpr const char* kNone = "none";
constexpr const char* kWildCard = "*";

const char* ItemTypeToString(net::structured_headers::Item::ItemType type) {
  switch (type) {
    case net::structured_headers::Item::ItemType::kIntegerType:
      return "Integer";
    case net::structured_headers::Item::ItemType::kDecimalType:
      return "Decimal";
    case net::structured_headers::Item::ItemType::kBooleanType:
      return "Boolean";
    case net::structured_headers::Item::ItemType::kByteSequenceType:
      return "ByteSequence";
    case net::structured_headers::Item::ItemType::kNullType:
      return "Null";
    case net::structured_headers::Item::ItemType::kStringType:
      return "String";
    case net::structured_headers::Item::ItemType::kTokenType:
      return "Token";
  }
}

const char* PolicyValueTypeToString(mojom::blink::PolicyValueType type) {
  switch (type) {
    case mojom::blink::PolicyValueType::kNull:
      return "Null";
    case mojom::blink::PolicyValueType::kBool:
      return "Boolean";
    case mojom::blink::PolicyValueType::kDecDouble:
      return "Double";
  }
}

base::Optional<PolicyValue> ItemToPolicyValue(
    const net::structured_headers::Item& item,
    mojom::blink::PolicyValueType type) {
  switch (type) {
    case mojom::blink::PolicyValueType::kDecDouble:
      switch (item.Type()) {
        case net::structured_headers::Item::ItemType::kIntegerType:
          return PolicyValue(static_cast<double>(item.GetInteger()));
        case net::structured_headers::Item::ItemType::kDecimalType:
          return PolicyValue(item.GetDecimal());
        default:
          return base::nullopt;
      }
    default:
      return base::nullopt;
  }
}

base::Optional<std::string> ItemToString(
    const net::structured_headers::Item& item) {
  if (item.Type() != net::structured_headers::Item::ItemType::kTokenType)
    return base::nullopt;
  return item.GetString();
}

struct ParsedFeature {
  mojom::blink::DocumentPolicyFeature feature;
  PolicyValue policy_value;
  base::Optional<std::string> endpoint_group;
  // Wildcard feature('*') is used to specify default endpoint for features.
  bool is_wildcard = false;
};

base::Optional<ParsedFeature> ParseWildcardFeature(
    const net::structured_headers::ParameterizedMember& directive,
    PolicyParserMessageBuffer& logger) {
  base::Optional<std::string> endpoint_group;

  for (const auto& param : directive.params) {
    if (param.first == kReportTo) {
      endpoint_group = ItemToString(param.second);
      if (!endpoint_group) {
        logger.Warn("\"report-to\" parameter should be a token.");
        return base::nullopt;
      }
    } else {
      // Unrecognized param.
      logger.Warn(String::Format("Unrecognized parameter name %s for %s.",
                                 param.first.c_str(), kWildCard));
    }
  }

  return base::make_optional<ParsedFeature>(
      {mojom::blink::DocumentPolicyFeature::kDefault, PolicyValue(),
       endpoint_group, true});
}

base::Optional<ParsedFeature> ParseFeature(
    const net::structured_headers::ParameterizedMember& directive,
    const DocumentPolicyNameFeatureMap& name_feature_map,
    const DocumentPolicyFeatureInfoMap& feature_info_map,
    PolicyParserMessageBuffer& logger) {
  ParsedFeature parsed_feature;

  if (directive.member_is_inner_list) {
    logger.Warn("Directives must not be inner lists.");
    return base::nullopt;
  }

  const net::structured_headers::Item& feature_token =
      directive.member.front().item;

  if (!feature_token.is_token()) {
    logger.Warn("The item in directive should be token type.");
    return base::nullopt;
  }

  const std::string& feature_name = feature_token.GetString();

  if (feature_name == kWildCard)
    return ParseWildcardFeature(directive, logger);

  auto feature_iter = name_feature_map.find(feature_name);

  // Parse feature_name string to DocumentPolicyFeature.
  if (feature_iter != name_feature_map.end()) {
    parsed_feature.feature = feature_iter->second;
  } else if (feature_name.size() > 3 && feature_name.substr(0, 3) == "no-") {
    // Handle "no-" prefix.
    feature_iter = name_feature_map.find(feature_name.substr(3));
    if (feature_iter != name_feature_map.end()) {
      parsed_feature.feature = feature_iter->second;
      if (feature_info_map.at(parsed_feature.feature).default_value.Type() !=
          mojom::blink::PolicyValueType::kBool) {
        logger.Warn(String::Format(
            "\"no-\" prefix is exclusively for policy with boolean policy "
            "value. Feature %s does not support boolean policy value.",
            feature_name.c_str()));
        return base::nullopt;
      }

      parsed_feature.policy_value = PolicyValue(false);
    } else {
      logger.Warn(
          String::Format("Unrecognized document policy feature name %s.",
                         feature_name.c_str()));
      return base::nullopt;
    }
  } else {
    logger.Warn(String::Format("Unrecognized document policy feature name %s.",
                               feature_name.c_str()));
    return base::nullopt;
  }

  auto expected_policy_value_type =
      feature_info_map.at(parsed_feature.feature).default_value.Type();
  // Handle boolean value.
  // For document policy that has a boolean policy value, policy value is not
  // specified as directive param. Instead, the value is expressed using "no-"
  // prefix, e.g. for feature X, "X" itself in header should be parsed as true,
  // "no-X" should be parsed as false.
  if (expected_policy_value_type == mojom::blink::PolicyValueType::kBool &&
      parsed_feature.policy_value.Type() ==
          mojom::blink::PolicyValueType::kNull)
    parsed_feature.policy_value = PolicyValue(true);

  const std::string& policy_value_param_name =
      feature_info_map.at(parsed_feature.feature).feature_param_name;
  for (const auto& param : directive.params) {
    const std::string& param_name = param.first;
    // Handle "report-to" param. "report-to" is an optional param for
    // Document-Policy header that specifies the endpoint group that the policy
    // should send report to. If left unspecified, no report will be send upon
    // policy violation.
    if (param_name == kReportTo) {
      parsed_feature.endpoint_group = ItemToString(param.second);
      if (!parsed_feature.endpoint_group) {
        logger.Warn(String::Format(
            "\"report-to\" parameter should be a token in feature %s.",
            feature_name.c_str()));
        return base::nullopt;
      }
    } else if (param_name == policy_value_param_name) {
      // Handle policy value. For all non-boolean policy value types, they
      // should be specified as FeatureX;f=xxx, with f representing the
      // |feature_param_name| and xxx representing policy value.

      // |parsed_feature.policy_value| should not be assigned yet.
      DCHECK(parsed_feature.policy_value.Type() ==
             mojom::blink::PolicyValueType::kNull);

      base::Optional<PolicyValue> policy_value =
          ItemToPolicyValue(param.second, expected_policy_value_type);
      if (!policy_value) {
        logger.Warn(String::Format(
            "Parameter %s in feature %s should be %s, but get %s.",
            policy_value_param_name.c_str(), feature_name.c_str(),
            PolicyValueTypeToString(expected_policy_value_type),
            ItemTypeToString(param.second.Type())));
        return base::nullopt;
      }
      parsed_feature.policy_value = *policy_value;
    } else {
      // Unrecognized param.
      logger.Warn(
          String::Format("Unrecognized parameter name %s for feature %s.",
                         param_name.c_str(), feature_name.c_str()));
    }
  }

  // |parsed_feature.policy_value| should be initialized.
  if (parsed_feature.policy_value.Type() ==
      mojom::blink::PolicyValueType::kNull) {
    logger.Warn(String::Format(
        "Policy value parameter missing for feature %s. Expected "
        "something like \"%s;%s=...\".",
        feature_name.c_str(), feature_name.c_str(),
        policy_value_param_name.c_str()));
    return base::nullopt;
  }

  return parsed_feature;
}

// Apply |default_endpoint| to given |parsed_policy|.
void ApplyDefaultEndpoint(DocumentPolicy::ParsedDocumentPolicy& parsed_policy,
                          const std::string& default_endpoint) {
  DocumentPolicy::FeatureEndpointMap& endpoint_map = parsed_policy.endpoint_map;

  if (!default_endpoint.empty()) {
    // Fill |default_endpoint| to all feature entry whose |endpoint_group|
    // is missing.
    for (const auto& feature_and_value : parsed_policy.feature_state) {
      mojom::blink::DocumentPolicyFeature feature = feature_and_value.first;

      if (endpoint_map.find(feature) == endpoint_map.end())
        endpoint_map.emplace(feature, default_endpoint);
    }
  }

  // Remove |endpoint_group| for feature entry if its |endpoint_group|
  // is "none".
  // Note: if |default_endpoint| is "none", all "none" items are filtered out
  // here. it would be equivalent to doing nothing.
  for (auto iter = endpoint_map.begin(); iter != endpoint_map.end();) {
    if (iter->second == kNone) {
      iter = endpoint_map.erase(iter);
    } else {
      ++iter;
    }
  }
}

}  // namespace

// static
base::Optional<DocumentPolicy::ParsedDocumentPolicy>
DocumentPolicyParser::Parse(const String& policy_string,
                            PolicyParserMessageBuffer& logger) {
  if (policy_string.IsEmpty())
    return base::make_optional<DocumentPolicy::ParsedDocumentPolicy>({});

  return ParseInternal(policy_string, GetDocumentPolicyNameFeatureMap(),
                       GetDocumentPolicyFeatureInfoMap(),
                       GetAvailableDocumentPolicyFeatures(), logger);
}

// static
base::Optional<DocumentPolicy::ParsedDocumentPolicy>
DocumentPolicyParser::ParseInternal(
    const String& policy_string,
    const DocumentPolicyNameFeatureMap& name_feature_map,
    const DocumentPolicyFeatureInfoMap& feature_info_map,
    const DocumentPolicyFeatureSet& available_features,
    PolicyParserMessageBuffer& logger) {
  auto root = net::structured_headers::ParseList(policy_string.Ascii());
  if (!root) {
    logger.Error(
        "Parse of document policy failed because of errors reported by "
        "structured header parser.");
    return base::nullopt;
  }

  DocumentPolicy::ParsedDocumentPolicy parse_result;
  std::string default_endpoint = "";
  for (const net::structured_headers::ParameterizedMember& directive :
       root.value()) {
    base::Optional<ParsedFeature> parsed_feature_option =
        ParseFeature(directive, name_feature_map, feature_info_map, logger);
    // If a feature fails parsing, ignore the entry.
    if (!parsed_feature_option)
      continue;

    ParsedFeature parsed_feature = *parsed_feature_option;

    if (parsed_feature.is_wildcard) {
      default_endpoint = *parsed_feature.endpoint_group;
      continue;
    }

    // If feature is not available, i.e. not enabled, ignore the entry.
    if (available_features.find(parsed_feature.feature) ==
        available_features.end())
      continue;

    parse_result.feature_state.emplace(parsed_feature.feature,
                                       std::move(parsed_feature.policy_value));
    if (parsed_feature.endpoint_group) {
      parse_result.endpoint_map.emplace(parsed_feature.feature,
                                        *parsed_feature.endpoint_group);
    }
  }

  ApplyDefaultEndpoint(parse_result, default_endpoint);

  return parse_result;
}

}  // namespace blink
