// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/instance_id_get_token_request_handler.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "google_apis/gcm/base/gcm_util.h"
#include "net/url_request/url_request_context_getter.h"

namespace gcm {

namespace {

// Request constants.
const char kAuthorizedEntityKey[] = "sender";
const char kGMSVersionKey[] = "gmsv";
const char kInstanceIDKey[] = "appid";
const char kScopeKey[] = "scope";
const char kExtraScopeKey[] = "X-scope";
const char kTimeToLiveSecondsKey[] = "ttl";
// Prefix that needs to be added for each option key.
const char kOptionKeyPrefix[] = "X-";

const base::Feature kSyncInstanceIDTokenTTL{"SyncInstanceIDTokenTTL",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPolicyInstanceIDTokenTTL{
    "PolicyInstanceIDTokenTTL", base::FEATURE_DISABLED_BY_DEFAULT};

// Default TTL (if the SyncInstanceIDTokenTTL/PolicyInstanceIDTokenTTL feature
// is enabled) is 4 weeks.
const int kDefaultInstanceIDTokenTTLSeconds = 28 * 24 * 60 * 60;

// Returns the TTL (time-to-live) for the Instance ID token, or 0 if no TTL
// should be specified.
// TODO(crbug.com/1030691): Properly expose specifying a TTL through the
// InstanceID API. For now, this is a quick hack to verify that this actually
// works and solves the problem.
base::TimeDelta GetTimeToLive(const std::string& authorized_entity) {
  // This magic value is identical to kInvalidationGCMSenderId, i.e. the value
  // that Sync uses for its invalidations.
  if (authorized_entity == "8181035976") {
    if (!base::FeatureList::IsEnabled(kSyncInstanceIDTokenTTL)) {
      return base::TimeDelta();
    }

    int time_to_live_seconds = base::GetFieldTrialParamByFeatureAsInt(
        kSyncInstanceIDTokenTTL, "time_to_live_seconds",
        kDefaultInstanceIDTokenTTLSeconds);
    return base::TimeDelta::FromSeconds(time_to_live_seconds);
  }

  // This magic value is identical to kPolicyFCMInvalidationSenderID, i.e. the
  // value that ChromeOS policy uses for its invalidations.
  if (authorized_entity == "1013309121859") {
    if (!base::FeatureList::IsEnabled(kPolicyInstanceIDTokenTTL)) {
      return base::TimeDelta();
    }

    int time_to_live_seconds = base::GetFieldTrialParamByFeatureAsInt(
        kPolicyInstanceIDTokenTTL, "time_to_live_seconds",
        kDefaultInstanceIDTokenTTLSeconds);
    return base::TimeDelta::FromSeconds(time_to_live_seconds);
  }

  // The default for all other FCM clients is no TTL.
  return base::TimeDelta();
}

}  // namespace

InstanceIDGetTokenRequestHandler::InstanceIDGetTokenRequestHandler(
    const std::string& instance_id,
    const std::string& authorized_entity,
    const std::string& scope,
    int gcm_version,
    const std::map<std::string, std::string>& options)
    : instance_id_(instance_id),
      authorized_entity_(authorized_entity),
      scope_(scope),
      gcm_version_(gcm_version),
      options_(options) {
  DCHECK(!instance_id.empty());
  DCHECK(!authorized_entity.empty());
  DCHECK(!scope.empty());
}

InstanceIDGetTokenRequestHandler::~InstanceIDGetTokenRequestHandler() {}

void InstanceIDGetTokenRequestHandler::BuildRequestBody(std::string* body) {
  BuildFormEncoding(kScopeKey, scope_, body);
  BuildFormEncoding(kExtraScopeKey, scope_, body);
  for (auto iter = options_.begin(); iter != options_.end(); ++iter)
    BuildFormEncoding(kOptionKeyPrefix + iter->first, iter->second, body);
  BuildFormEncoding(kGMSVersionKey, base::NumberToString(gcm_version_), body);
  BuildFormEncoding(kInstanceIDKey, instance_id_, body);
  BuildFormEncoding(kAuthorizedEntityKey, authorized_entity_, body);
  base::TimeDelta ttl = GetTimeToLive(authorized_entity_);
  if (!ttl.is_zero()) {
    BuildFormEncoding(kTimeToLiveSecondsKey,
                      base::NumberToString(ttl.InSeconds()), body);
  }
}

void InstanceIDGetTokenRequestHandler::ReportStatusToUMA(
    RegistrationRequest::Status status) {
  UMA_HISTOGRAM_ENUMERATION("InstanceID.GetToken.RequestStatus", status,
                            RegistrationRequest::STATUS_COUNT);
}

void InstanceIDGetTokenRequestHandler::ReportNetErrorCodeToUMA(
    int net_error_code) {
  base::UmaHistogramSparse("InstanceID.GetToken.NetErrorCode",
                           std::abs(net_error_code));
}

}  // namespace gcm
