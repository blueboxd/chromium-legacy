// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"

#include <memory>
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "third_party/blink/public/common/origin_trials/origin_trial_policy.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"

namespace {

static base::RepeatingCallback<blink::OriginTrialPolicy*()>& PolicyGetter() {
  static base::NoDestructor<
      base::RepeatingCallback<blink::OriginTrialPolicy*()>>
      policy(base::BindRepeating(
          []() -> blink::OriginTrialPolicy* { return nullptr; }));
  return *policy;
}
}  // namespace

namespace blink {

TrialTokenValidator::TrialTokenValidator() {}

TrialTokenValidator::~TrialTokenValidator() = default;

void TrialTokenValidator::SetOriginTrialPolicyGetter(
    base::RepeatingCallback<OriginTrialPolicy*()> policy_getter) {
  PolicyGetter() = policy_getter;
}

void TrialTokenValidator::ResetOriginTrialPolicyGetter() {
  SetOriginTrialPolicyGetter(base::BindRepeating(
      []() -> blink::OriginTrialPolicy* { return nullptr; }));
}

OriginTrialPolicy* TrialTokenValidator::Policy() {
  return PolicyGetter().Run();
}

OriginTrialTokenStatus TrialTokenValidator::ValidateToken(
    base::StringPiece token,
    const url::Origin& origin,
    base::Time current_time,
    std::string* feature_name,
    base::Time* expiry_time) const {
  OriginTrialPolicy* policy = Policy();

  if (!policy->IsOriginTrialsSupported())
    return OriginTrialTokenStatus::kNotSupported;

  std::vector<base::StringPiece> public_keys = policy->GetPublicKeys();
  if (public_keys.size() == 0)
    return OriginTrialTokenStatus::kNotSupported;

  OriginTrialTokenStatus status;
  std::unique_ptr<TrialToken> trial_token;
  for (auto& key : public_keys) {
    trial_token = TrialToken::From(token, key, &status);
    if (status == OriginTrialTokenStatus::kSuccess)
      break;
  }

  if (status != OriginTrialTokenStatus::kSuccess)
    return status;

  status = trial_token->IsValid(origin, current_time);
  if (status != OriginTrialTokenStatus::kSuccess)
    return status;

  if (policy->IsFeatureDisabled(trial_token->feature_name()))
    return OriginTrialTokenStatus::kFeatureDisabled;

  if (policy->IsTokenDisabled(trial_token->signature()))
    return OriginTrialTokenStatus::kTokenDisabled;

  *feature_name = trial_token->feature_name();
  *expiry_time = trial_token->expiry_time();
  return OriginTrialTokenStatus::kSuccess;
}

bool TrialTokenValidator::RequestEnablesFeature(const net::URLRequest* request,
                                                base::StringPiece feature_name,
                                                base::Time current_time) const {
  // TODO(mek): Possibly cache the features that are availble for request in
  // UserData associated with the request.
  return RequestEnablesFeature(request->url(), request->response_headers(),
                               feature_name, current_time);
}

bool TrialTokenValidator::RequestEnablesFeature(
    const GURL& request_url,
    const net::HttpResponseHeaders* response_headers,
    base::StringPiece feature_name,
    base::Time current_time) const {
  if (!IsTrialPossibleOnOrigin(request_url))
    return false;

  url::Origin origin = url::Origin::Create(request_url);
  size_t iter = 0;
  std::string token;
  while (response_headers->EnumerateHeader(&iter, "Origin-Trial", &token)) {
    std::string token_feature;
    base::Time expiry_time;
    // TODO(mek): Log the validation errors to histograms?
    if (ValidateToken(token, origin, current_time, &token_feature,
                      &expiry_time) == OriginTrialTokenStatus::kSuccess)
      if (token_feature == feature_name)
        return true;
  }
  return false;
}

std::unique_ptr<TrialTokenValidator::FeatureToTokensMap>
TrialTokenValidator::GetValidTokensFromHeaders(
    const url::Origin& origin,
    const net::HttpResponseHeaders* headers,
    base::Time current_time) const {
  std::unique_ptr<FeatureToTokensMap> tokens(
      std::make_unique<FeatureToTokensMap>());
  if (!IsTrialPossibleOnOrigin(origin.GetURL()))
    return tokens;

  size_t iter = 0;
  std::string token;
  while (headers->EnumerateHeader(&iter, "Origin-Trial", &token)) {
    std::string token_feature;
    base::Time expiry_time;
    if (TrialTokenValidator::ValidateToken(token, origin, current_time,
                                           &token_feature, &expiry_time) ==
        OriginTrialTokenStatus::kSuccess) {
      (*tokens)[token_feature].push_back(token);
    }
  }
  return tokens;
}

std::unique_ptr<TrialTokenValidator::FeatureToTokensMap>
TrialTokenValidator::GetValidTokens(const url::Origin& origin,
                                    const FeatureToTokensMap& tokens,
                                    base::Time current_time) const {
  std::unique_ptr<FeatureToTokensMap> out_tokens(
      std::make_unique<FeatureToTokensMap>());
  if (!IsTrialPossibleOnOrigin(origin.GetURL()))
    return out_tokens;

  for (const auto& feature : tokens) {
    for (const std::string& token : feature.second) {
      std::string token_feature;
      base::Time expiry_time;
      if (TrialTokenValidator::ValidateToken(token, origin, current_time,
                                             &token_feature, &expiry_time) ==
          OriginTrialTokenStatus::kSuccess) {
        DCHECK_EQ(token_feature, feature.first);
        (*out_tokens)[feature.first].push_back(token);
      }
    }
  }
  return out_tokens;
}

// static
bool TrialTokenValidator::IsTrialPossibleOnOrigin(const GURL& url) {
  OriginTrialPolicy* policy = Policy();
  return policy && policy->IsOriginTrialsSupported() &&
         policy->IsOriginSecure(url);
}

}  // namespace blink
