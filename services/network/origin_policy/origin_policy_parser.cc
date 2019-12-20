// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/origin_policy/origin_policy_parser.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

OriginPolicyContentsPtr OriginPolicyParser::Parse(base::StringPiece text) {
  OriginPolicyParser parser;
  if (!parser.DoParse(text))
    return std::make_unique<OriginPolicyContents>();
  return std::move(parser.policy_contents_);
}

OriginPolicyParser::OriginPolicyParser() {}
OriginPolicyParser::~OriginPolicyParser() {}

bool OriginPolicyParser::DoParse(base::StringPiece policy_contents_text) {
  if (policy_contents_text.empty())
    return false;

  std::unique_ptr<base::Value> json =
      base::JSONReader::ReadDeprecated(policy_contents_text);
  if (!json || !json->is_dict())
    return false;

  policy_contents_ = std::make_unique<OriginPolicyContents>();

  base::Value* csp = json->FindListKey("content-security-policy");
  bool csp_ok = !csp || ParseContentSecurityPolicies(*csp);

  base::Value* features = json->FindDictKey("features");
  if (features) {
    ParseFeatures(*features);
  }

  return csp_ok;
}

bool OriginPolicyParser::ParseContentSecurityPolicies(
    const base::Value& policies) {
  bool ok = true;
  for (const auto& csp : policies.GetList()) {
    ok &= csp.is_dict() && ParseContentSecurityPolicy(csp);
  }
  return ok;
}

bool OriginPolicyParser::ParseContentSecurityPolicy(const base::Value& csp) {
  const std::string* policy = csp.FindStringKey("policy");
  if (!policy)
    return false;

  const base::Optional<bool> report_only = csp.FindBoolKey("report-only");

  if (report_only.has_value() && report_only.value()) {
    policy_contents_->content_security_policies_report_only.push_back(*policy);
  } else {
    policy_contents_->content_security_policies.push_back(*policy);
  }

  return true;
}

void OriginPolicyParser::ParseFeatures(const base::Value& features) {
  const std::string* policy = features.FindStringKey("policy");

  if (policy) {
    policy_contents_->feature_policy = *policy;
  }
}

}  // namespace network
