// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/origin_policy/origin_policy_manager.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/optional.h"
#include "net/http/http_util.h"
#include "services/network/origin_policy/origin_policy_fetcher.h"

namespace {

// Marker for (temporarily) exempted origins. The presence of the "?" guarantees
// that this is not a valid policy as it is not a valid http token.
const char kExemptedOriginPolicyVersion[] = "exception?";

}  // namespace

namespace network {

OriginPolicyManager::OriginPolicyManager(
    mojom::URLLoaderFactoryPtr url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

OriginPolicyManager::~OriginPolicyManager() {}

void OriginPolicyManager::AddBinding(
    mojom::OriginPolicyManagerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void OriginPolicyManager::RetrieveOriginPolicy(
    const url::Origin& origin,
    const std::string& header_value,
    RetrieveOriginPolicyCallback callback) {
  DCHECK(origin.GetURL().is_valid());
  DCHECK(!origin.opaque());

  OriginPolicyHeaderValues header_info =
      GetRequestedPolicyAndReportGroupFromHeaderString(header_value);

  auto iter = latest_version_map_.find(origin);

  // Process policy deletion first!
  if (header_info.policy_version == kOriginPolicyDeletePolicy) {
    if (iter != latest_version_map_.end())
      latest_version_map_.erase(iter);
    InvokeCallbackWithPolicyState(origin,
                                  mojom::OriginPolicyState::kNoPolicyApplies,
                                  std::move(callback));
    return;
  }

  // Process policy exceptions.
  if (iter != latest_version_map_.end() &&
      iter->second == kExemptedOriginPolicyVersion) {
    InvokeCallbackWithPolicyState(origin,
                                  mojom::OriginPolicyState::kNoPolicyApplies,
                                  std::move(callback));
    return;
  }

  // No policy applies to this request or invalid header present.
  if (header_info.policy_version.empty()) {
    // If there header has no policy version is present, use cached version, if
    // there is one. Otherwise, fail.
    if (iter == latest_version_map_.end()) {
      InvokeCallbackWithPolicyState(
          origin,
          header_value.empty() ? mojom::OriginPolicyState::kNoPolicyApplies
                               : mojom::OriginPolicyState::kCannotLoadPolicy,
          std::move(callback));
      return;
    }
    header_info.policy_version = iter->second;
  } else if (iter == latest_version_map_.end()) {
    latest_version_map_.emplace(origin, header_info.policy_version);
  } else {
    iter->second = header_info.policy_version;
  }

  origin_policy_fetchers_.emplace(std::make_unique<OriginPolicyFetcher>(
      this, header_info.policy_version, header_info.report_to, origin,
      url_loader_factory_.get(), std::move(callback)));
}

void OriginPolicyManager::AddExceptionFor(const url::Origin& origin) {
  latest_version_map_[origin] = kExemptedOriginPolicyVersion;
}

void OriginPolicyManager::FetcherDone(OriginPolicyFetcher* fetcher,
                                      mojom::OriginPolicyPtr origin_policy,
                                      RetrieveOriginPolicyCallback callback) {
  std::move(callback).Run(std::move(origin_policy));

  auto it = origin_policy_fetchers_.find(fetcher);
  DCHECK(it != origin_policy_fetchers_.end());
  origin_policy_fetchers_.erase(it);
}

void OriginPolicyManager::RetrieveDefaultOriginPolicy(
    const url::Origin& origin,
    RetrieveOriginPolicyCallback callback) {
  origin_policy_fetchers_.emplace(std::make_unique<OriginPolicyFetcher>(
      this, std::string() /* report_to */, origin, url_loader_factory_.get(),
      std::move(callback)));
}

// static
const char* OriginPolicyManager::GetExemptedVersionForTesting() {
  return kExemptedOriginPolicyVersion;
}

// static
OriginPolicyManager::OriginPolicyHeaderValues
OriginPolicyManager::GetRequestedPolicyAndReportGroupFromHeaderString(
    const std::string& header_value) {
  if (net::HttpUtil::TrimLWS(header_value) == kOriginPolicyDeletePolicy)
    return OriginPolicyHeaderValues({kOriginPolicyDeletePolicy, ""});

  base::Optional<std::string> policy;
  base::Optional<std::string> report_to;
  bool valid = true;
  net::HttpUtil::NameValuePairsIterator iter(header_value.cbegin(),
                                             header_value.cend(), ',');
  while (iter.GetNext()) {
    std::string token_value = net::HttpUtil::TrimLWS(iter.value()).as_string();
    bool is_token = net::HttpUtil::IsToken(token_value);
    if (iter.name() == kOriginPolicyPolicy) {
      valid &= is_token && !policy.has_value();
      policy = token_value;
    } else if (iter.name() == kOriginPolicyReportTo) {
      valid &= is_token && !report_to.has_value();
      report_to = token_value;
    }
  }
  valid &= iter.valid();
  valid &= (policy.has_value() && policy->find('.') == std::string::npos);

  if (!valid)
    return OriginPolicyHeaderValues();
  return OriginPolicyHeaderValues({policy.value(), report_to.value_or("")});
}

// static
void OriginPolicyManager::InvokeCallbackWithPolicyState(
    const url::Origin& origin,
    mojom::OriginPolicyState state,
    RetrieveOriginPolicyCallback callback) {
  mojom::OriginPolicyPtr result = mojom::OriginPolicy::New();
  result->state = state;
  result->policy_url = OriginPolicyFetcher::GetDefaultPolicyURL(origin);
  std::move(callback).Run(std::move(result));
}

}  // namespace network
