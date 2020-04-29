// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/feature_policy/feature_policy_parser.h"

#include <algorithm>
#include <utility>

#include <bitset>
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "url/origin.h"

namespace blink {
namespace {

class ParsingContext {
 public:
  ParsingContext(Vector<String>* messages,
                 scoped_refptr<const SecurityOrigin> self_origin,
                 scoped_refptr<const SecurityOrigin> src_origin,
                 const FeatureNameMap& feature_names,
                 FeaturePolicyParserDelegate* delegate)
      : messages_(messages),
        self_origin_(self_origin),
        src_origin_(src_origin),
        feature_names_(feature_names),
        delegate_(delegate) {}

  ~ParsingContext() = default;

  ParsedFeaturePolicy Parse(const String& policy);

 private:
  // normally 1 char = 1 byte
  // max length to parse = 2^16 = 64 kB
  static constexpr wtf_size_t MAX_LENGTH_PARSE = 1 << 16;

  // Parse a single feature entry. e.g. feature_a ORIGIN_A ORIGIN_B.
  // feature_entry = feature_name ' ' allowlist
  base::Optional<ParsedFeaturePolicyDeclaration> ParseFeature(
      const String& input);

  struct ParsedAllowlist {
    std::vector<url::Origin> allowed_origins;
    bool fallback_value;
    bool opaque_value;

    ParsedAllowlist()
        : allowed_origins({}), fallback_value(false), opaque_value(false) {}
  };

  base::Optional<mojom::blink::FeaturePolicyFeature> ParseFeatureName(
      const String& feature_name);

  // Parse allowlist for feature.
  ParsedAllowlist ParseAllowlist(const Vector<String>& origin_strings);

  inline void Log(const String& message) {
    if (messages_)
      messages_->push_back(message);
  }

  bool FeatureObserved(mojom::blink::FeaturePolicyFeature feature);

  void ReportFeaturePolicyWebFeatureUsage(mojom::blink::WebFeature feature);

  void ReportFeatureUsage(mojom::blink::FeaturePolicyFeature feature);

  // This function should be called after Allowlist Histograms related flags
  // have been captured.
  void RecordAllowlistTypeUsage(size_t origin_count);
  // The use of various allowlist types should only be recorded once per page.
  // For simplicity, this recording assumes that the ParseHeader method is
  // called once when creating a new document, and similarly the ParseAttribute
  // method is called once for a frame. It is possible for multiple calls, but
  // the additional complexity to guarantee only one record isn't warranted as
  // yet.
  void ReportAllowlistTypeUsage();

  Vector<String>* messages_;
  scoped_refptr<const SecurityOrigin> self_origin_;
  scoped_refptr<const SecurityOrigin> src_origin_;
  const FeatureNameMap& feature_names_;
  FeaturePolicyParserDelegate* delegate_;

  // Flags for the types of items which can be used in allowlists.
  bool allowlist_includes_star_ = false;
  bool allowlist_includes_self_ = false;
  bool allowlist_includes_src_ = false;
  bool allowlist_includes_none_ = false;
  bool allowlist_includes_origin_ = false;

  HashSet<FeaturePolicyAllowlistType> allowlist_types_used_;
  std::bitset<
      static_cast<size_t>(mojom::blink::FeaturePolicyFeature::kMaxValue) + 1>
      features_specified_;
};

bool ParsingContext::FeatureObserved(
    mojom::blink::FeaturePolicyFeature feature) {
  if (features_specified_[static_cast<size_t>(feature)]) {
    return true;
  } else {
    features_specified_.set(static_cast<size_t>(feature));
    return false;
  }
}

void ParsingContext::ReportFeaturePolicyWebFeatureUsage(
    mojom::blink::WebFeature feature) {
  if (delegate_)
    delegate_->CountFeaturePolicyUsage(feature);
}

void ParsingContext::ReportFeatureUsage(
    mojom::blink::FeaturePolicyFeature feature) {
  if (src_origin_) {
    if (!delegate_ || !delegate_->FeaturePolicyFeatureObserved(feature)) {
      UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.FeaturePolicy.Allow",
                                feature);
    }
  } else {
    UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.FeaturePolicy.Header", feature);
  }
}

void ParsingContext::RecordAllowlistTypeUsage(size_t origin_count) {
  // Record the type of allowlist used.
  if (origin_count == 0) {
    allowlist_types_used_.insert(FeaturePolicyAllowlistType::kEmpty);
  } else if (origin_count == 1) {
    if (allowlist_includes_star_)
      allowlist_types_used_.insert(FeaturePolicyAllowlistType::kStar);
    else if (allowlist_includes_self_)
      allowlist_types_used_.insert(FeaturePolicyAllowlistType::kSelf);
    else if (allowlist_includes_src_)
      allowlist_types_used_.insert(FeaturePolicyAllowlistType::kSrc);
    else if (allowlist_includes_none_)
      allowlist_types_used_.insert(FeaturePolicyAllowlistType::kNone);
    else
      allowlist_types_used_.insert(FeaturePolicyAllowlistType::kOrigins);
  } else {
    if (allowlist_includes_origin_) {
      if (allowlist_includes_star_ || allowlist_includes_none_ ||
          allowlist_includes_src_ || allowlist_includes_self_)
        allowlist_types_used_.insert(FeaturePolicyAllowlistType::kMixed);
      else
        allowlist_types_used_.insert(FeaturePolicyAllowlistType::kOrigins);
    } else {
      allowlist_types_used_.insert(FeaturePolicyAllowlistType::kKeywordsOnly);
    }
  }
  // Reset all flags.
  allowlist_includes_star_ = false;
  allowlist_includes_self_ = false;
  allowlist_includes_src_ = false;
  allowlist_includes_none_ = false;
  allowlist_includes_origin_ = false;
}

void ParsingContext::ReportAllowlistTypeUsage() {
  for (const FeaturePolicyAllowlistType allowlist_type :
       allowlist_types_used_) {
    if (src_origin_) {
      UMA_HISTOGRAM_ENUMERATION(
          "Blink.UseCounter.FeaturePolicy.AttributeAllowlistType",
          allowlist_type);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Blink.UseCounter.FeaturePolicy.HeaderAllowlistType", allowlist_type);
    }
  }
}

base::Optional<mojom::blink::FeaturePolicyFeature>
ParsingContext::ParseFeatureName(const String& feature_name) {
  DCHECK(!feature_name.IsEmpty());
  if (!feature_names_.Contains(feature_name)) {
    Log("Unrecognized feature: '" + feature_name + "'.");
    return base::nullopt;
  }
  if (DisabledByOriginTrial(feature_name, delegate_)) {
    Log("Origin trial controlled feature not enabled: '" + feature_name + "'.");
    return base::nullopt;
  }
  mojom::blink::FeaturePolicyFeature feature = feature_names_.at(feature_name);

  return feature;
}

ParsingContext::ParsedAllowlist ParsingContext::ParseAllowlist(
    const Vector<String>& origin_strings) {
  ParsedAllowlist allowlist;
  if (origin_strings.IsEmpty()) {
    // If a policy entry has no listed origins (e.g. "feature_name1" in
    // allow="feature_name1; feature_name2 value"), enable the feature for:
    //     a. |self_origin|, if we are parsing a header policy (i.e.,
    //       |src_origin| is null);
    //     b. |src_origin|, if we are parsing an allow attribute (i.e.,
    //       |src_origin| is not null), |src_origin| is not opaque; or
    //     c. the opaque origin of the frame, if |src_origin| is opaque.
    if (!src_origin_) {
      allowlist.allowed_origins.push_back(self_origin_->ToUrlOrigin());
    } else if (!src_origin_->IsOpaque()) {
      allowlist.allowed_origins.push_back(src_origin_->ToUrlOrigin());
    } else {
      allowlist.opaque_value = true;
    }
  } else {
    for (const String& origin_string : origin_strings) {
      DCHECK(!origin_string.IsEmpty());

      if (!origin_string.ContainsOnlyASCIIOrEmpty()) {
        Log("Non-ASCII characters in origin.");
        continue;
      }

      // Determine the target of the declaration. This may be a specific
      // origin, either explicitly written, or one of the special keywords
      // 'self' or 'src'. ('src' can only be used in the iframe allow
      // attribute.)
      url::Origin target_origin;

      // If the iframe will have an opaque origin (for example, if it is
      // sandboxed, or has a data: URL), then 'src' needs to refer to the
      // opaque origin of the frame, which is not known yet. In this case,
      // the |opaque_value| on the declaration is set, rather than adding
      // an origin to the allowlist.
      bool target_is_opaque = false;
      bool target_is_all = false;

      // 'self' origin is used if the origin is exactly 'self'.
      if (EqualIgnoringASCIICase(origin_string, "'self'")) {
        allowlist_includes_self_ = true;
        target_origin = self_origin_->ToUrlOrigin();
      }
      // 'src' origin is used if |src_origin| is available and the
      // origin is a match for 'src'. |src_origin| is only set
      // when parsing an iframe allow attribute.
      else if (src_origin_ && EqualIgnoringASCIICase(origin_string, "'src'")) {
        allowlist_includes_src_ = true;
        if (!src_origin_->IsOpaque()) {
          target_origin = src_origin_->ToUrlOrigin();
        } else {
          target_is_opaque = true;
        }
      } else if (EqualIgnoringASCIICase(origin_string, "'none'")) {
        allowlist_includes_none_ = true;
        continue;
      } else if (origin_string == "*") {
        allowlist_includes_star_ = true;
        target_is_all = true;
      }
      // Otherwise, parse the origin string and verify that the result is
      // valid. Invalid strings will produce an opaque origin, which will
      // result in an error message.
      else {
        scoped_refptr<SecurityOrigin> parsed_origin =
            SecurityOrigin::CreateFromString(origin_string);
        if (!parsed_origin->IsOpaque()) {
          target_origin = parsed_origin->ToUrlOrigin();
          allowlist_includes_origin_ = true;
        } else {
          Log("Unrecognized origin: '" + origin_string + "'.");
          continue;
        }
      }

      if (target_is_all) {
        allowlist.fallback_value = true;
        allowlist.opaque_value = true;
      } else if (target_is_opaque) {
        allowlist.opaque_value = true;
      } else {
        allowlist.allowed_origins.push_back(target_origin);
      }
    }
  }

  // Size reduction: remove all items in the allowlist if target is all.
  if (allowlist.fallback_value)
    allowlist.allowed_origins.clear();

  // Sort |allowed_origins| in alphabetical order.
  std::sort(allowlist.allowed_origins.begin(), allowlist.allowed_origins.end());

  RecordAllowlistTypeUsage(origin_strings.size());

  return allowlist;
}

base::Optional<ParsedFeaturePolicyDeclaration> ParsingContext::ParseFeature(
    const String& input) {
  // Split removes extra whitespaces by default
  Vector<String> tokens;
  input.Split(' ', tokens);

  // Empty policy. Skip.
  if (tokens.IsEmpty())
    return base::nullopt;

  // Break tokens into head & tail, where
  // head = feature_name
  // tail = origins
  // After feature_name has been parsed, take tail of tokens vector by
  // erasing the first element.
  base::Optional<mojom::blink::FeaturePolicyFeature> feature =
      ParseFeatureName(/* feature_name */ tokens.front());

  tokens.erase(tokens.begin());

  ParsedAllowlist parsed_allowlist =
      ParseAllowlist(/* origin_strings */ tokens);

  if (!feature)
    return base::nullopt;

  // If same feature appeared more than once, only the first one counts.
  if (FeatureObserved(*feature))
    return base::nullopt;

  ParsedFeaturePolicyDeclaration parsed_feature(*feature);
  parsed_feature.allowed_origins = std::move(parsed_allowlist.allowed_origins);
  parsed_feature.fallback_value = parsed_allowlist.fallback_value;
  parsed_feature.opaque_value = parsed_allowlist.opaque_value;

  return parsed_feature;
}

ParsedFeaturePolicy ParsingContext::Parse(const String& policy) {
  ParsedFeaturePolicy parsed_policy;

  if (policy.length() > MAX_LENGTH_PARSE) {
    Log("Feature policy declaration exceeds size limit(" +
        String::Number(policy.length()) + ">" +
        String::Number(MAX_LENGTH_PARSE) + ")");
    return parsed_policy;
  }

  // RFC2616, section 4.2 specifies that headers appearing multiple times can be
  // combined with a comma. Walk the header string, and parse each comma
  // separated chunk as a separate header.
  Vector<String> policy_items;
  // policy_items = [ policy *( "," [ policy ] ) ]
  policy.Split(',', policy_items);

  if (policy_items.size() > 1) {
    ReportFeaturePolicyWebFeatureUsage(
        mojom::blink::WebFeature::kFeaturePolicyCommaSeparatedDeclarations);
  }

  for (const String& item : policy_items) {
    Vector<String> feature_entries;
    // feature_entries = [ feature_entry *( ";" [ feature_entry ] ) ]
    item.Split(';', feature_entries);

    if (feature_entries.size() > 1) {
      ReportFeaturePolicyWebFeatureUsage(
          mojom::blink::WebFeature::
              kFeaturePolicySemicolonSeparatedDeclarations);
    }

    for (const String& feature_entry : feature_entries) {
      base::Optional<ParsedFeaturePolicyDeclaration> parsed_feature =
          ParseFeature(feature_entry);
      if (parsed_feature) {
        ReportFeatureUsage(parsed_feature->feature);
        parsed_policy.push_back(*parsed_feature);
      }
    }
  }

  ReportAllowlistTypeUsage();

  return parsed_policy;
}

}  // namespace

ParsedFeaturePolicy FeaturePolicyParser::ParseHeader(
    const String& policy,
    scoped_refptr<const SecurityOrigin> origin,
    Vector<String>* messages,
    FeaturePolicyParserDelegate* delegate) {
  return Parse(policy, origin, nullptr, messages, GetDefaultFeatureNameMap(),
               delegate);
}

ParsedFeaturePolicy FeaturePolicyParser::ParseAttribute(
    const String& policy,
    scoped_refptr<const SecurityOrigin> self_origin,
    scoped_refptr<const SecurityOrigin> src_origin,
    Vector<String>* messages,
    Document* document) {
  return Parse(policy, self_origin, src_origin, messages,
               GetDefaultFeatureNameMap(), document);
}

// static
ParsedFeaturePolicy FeaturePolicyParser::Parse(
    const String& policy,
    scoped_refptr<const SecurityOrigin> self_origin,
    scoped_refptr<const SecurityOrigin> src_origin,
    Vector<String>* messages,
    const FeatureNameMap& feature_names,
    FeaturePolicyParserDelegate* delegate) {
  return ParsingContext(messages, self_origin, src_origin, feature_names,
                        delegate)
      .Parse(policy);
}

bool IsFeatureDeclared(mojom::blink::FeaturePolicyFeature feature,
                       const ParsedFeaturePolicy& policy) {
  return std::any_of(policy.begin(), policy.end(),
                     [feature](const auto& declaration) {
                       return declaration.feature == feature;
                     });
}

bool RemoveFeatureIfPresent(mojom::blink::FeaturePolicyFeature feature,
                            ParsedFeaturePolicy& policy) {
  auto new_end = std::remove_if(policy.begin(), policy.end(),
                                [feature](const auto& declaration) {
                                  return declaration.feature == feature;
                                });
  if (new_end == policy.end())
    return false;
  policy.erase(new_end, policy.end());
  return true;
}

bool DisallowFeatureIfNotPresent(mojom::blink::FeaturePolicyFeature feature,
                                 ParsedFeaturePolicy& policy) {
  if (IsFeatureDeclared(feature, policy))
    return false;
  ParsedFeaturePolicyDeclaration allowlist(feature);
  policy.push_back(allowlist);
  return true;
}

bool AllowFeatureEverywhereIfNotPresent(
    mojom::blink::FeaturePolicyFeature feature,
    ParsedFeaturePolicy& policy) {
  if (IsFeatureDeclared(feature, policy))
    return false;
  ParsedFeaturePolicyDeclaration allowlist(feature);
  allowlist.fallback_value = true;
  allowlist.opaque_value = true;
  policy.push_back(allowlist);
  return true;
}

void DisallowFeature(mojom::blink::FeaturePolicyFeature feature,
                     ParsedFeaturePolicy& policy) {
  RemoveFeatureIfPresent(feature, policy);
  DisallowFeatureIfNotPresent(feature, policy);
}

void AllowFeatureEverywhere(mojom::blink::FeaturePolicyFeature feature,
                            ParsedFeaturePolicy& policy) {
  RemoveFeatureIfPresent(feature, policy);
  AllowFeatureEverywhereIfNotPresent(feature, policy);
}

const Vector<String> GetAvailableFeatures(ExecutionContext* execution_context) {
  Vector<String> available_features;
  for (const auto& feature : GetDefaultFeatureNameMap()) {
    if (!DisabledByOriginTrial(feature.key, execution_context))
      available_features.push_back(feature.key);
  }
  return available_features;
}

const String& GetNameForFeature(mojom::blink::FeaturePolicyFeature feature) {
  for (const auto& entry : GetDefaultFeatureNameMap()) {
    if (entry.value == feature)
      return entry.key;
  }
  return g_empty_string;
}

}  // namespace blink
