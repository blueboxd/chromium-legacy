// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/client_policy_controller.h"

#include <utility>

#include "base/time/time.h"
#include "components/offline_pages/core/client_namespace_constants.h"

namespace offline_pages {

OfflinePageClientPolicy* ClientPolicyController::AddTemporaryPolicy(
    const std::string& name_space,
    const base::TimeDelta& expiration_period) {
  auto iter_and_was_inserted_pair = policies_.emplace(
      name_space, OfflinePageClientPolicy(name_space, LifetimeType::TEMPORARY));
  DCHECK(iter_and_was_inserted_pair.second) << "Policy was not inserted";
  OfflinePageClientPolicy& policy = (*iter_and_was_inserted_pair.first).second;
  policy.expiration_period = expiration_period;
  return &policy;
}

OfflinePageClientPolicy* ClientPolicyController::AddPersistentPolicy(
    const std::string& name_space) {
  auto iter_and_was_inserted_pair = policies_.emplace(
      name_space,
      OfflinePageClientPolicy(name_space, LifetimeType::PERSISTENT));
  DCHECK(iter_and_was_inserted_pair.second) << "Policy was not inserted";
  OfflinePageClientPolicy& policy = (*iter_and_was_inserted_pair.first).second;
  return &policy;
}

ClientPolicyController::ClientPolicyController() {
  {
    OfflinePageClientPolicy* policy =
        AddTemporaryPolicy(kBookmarkNamespace, base::TimeDelta::FromDays(7));
    policy->pages_allowed_per_url = 1;
  }
  {
    OfflinePageClientPolicy* policy =
        AddTemporaryPolicy(kLastNNamespace, base::TimeDelta::FromDays(30));
    policy->is_restricted_to_tab_from_client_id = true;
  }
  {
    OfflinePageClientPolicy* policy = AddPersistentPolicy(kAsyncNamespace);
    policy->is_supported_by_download = true;
  }
  {
    OfflinePageClientPolicy* policy =
        AddTemporaryPolicy(kCCTNamespace, base::TimeDelta::FromDays(2));
    policy->pages_allowed_per_url = 1;
    policy->requires_specific_user_settings = true;
  }
  {
    OfflinePageClientPolicy* policy = AddPersistentPolicy(kDownloadNamespace);
    policy->is_supported_by_download = true;
  }
  {
    OfflinePageClientPolicy* policy =
        AddPersistentPolicy(kNTPSuggestionsNamespace);
    policy->is_supported_by_download = true;
  }
  {
    OfflinePageClientPolicy* policy = AddTemporaryPolicy(
        kSuggestedArticlesNamespace, base::TimeDelta::FromDays(30));
    policy->is_supported_by_download = 1;
    policy->is_suggested = true;
  }
  {
    OfflinePageClientPolicy* policy =
        AddPersistentPolicy(kBrowserActionsNamespace);
    policy->is_supported_by_download = true;
    policy->allows_conversion_to_background_file_download = true;
  }
  {
    OfflinePageClientPolicy* policy = AddTemporaryPolicy(
        kLivePageSharingNamespace, base::TimeDelta::FromHours(1));
    policy->pages_allowed_per_url = 1;
    policy->is_restricted_to_tab_from_client_id = true;
  }
  {
    OfflinePageClientPolicy* policy =
        AddTemporaryPolicy(kAutoAsyncNamespace, base::TimeDelta::FromDays(30));
    policy->pages_allowed_per_url = 1;
    policy->defer_background_fetch_while_page_is_active = true;
  }

  // Fallback policy.
  {
    OfflinePageClientPolicy* policy =
        AddTemporaryPolicy(kDefaultNamespace, base::TimeDelta::FromDays(1));
    policy->page_limit = 10;
    policy->pages_allowed_per_url = 1;
  }

  for (const auto& policy_item : policies_) {
    const std::string& name = policy_item.first;
    switch (policy_item.second.lifetime_type) {
      case LifetimeType::TEMPORARY:
        temporary_namespaces_.push_back(name);
        break;
      case LifetimeType::PERSISTENT:
        persistent_namespaces_.push_back(name);
        break;
    }
  }
}

ClientPolicyController::~ClientPolicyController() {}

const OfflinePageClientPolicy& ClientPolicyController::GetPolicy(
    const std::string& name_space) const {
  const auto& iter = policies_.find(name_space);
  if (iter != policies_.end())
    return iter->second;
  // Fallback when the namespace isn't defined.
  return policies_.at(kDefaultNamespace);
}

std::vector<std::string> ClientPolicyController::GetAllNamespaces() const {
  std::vector<std::string> result;
  for (const auto& policy_item : policies_)
    result.emplace_back(policy_item.first);

  return result;
}

bool ClientPolicyController::IsTemporary(const std::string& name_space) const {
  return GetPolicy(name_space).lifetime_type == LifetimeType::TEMPORARY;
}

const std::vector<std::string>& ClientPolicyController::GetTemporaryNamespaces()
    const {
  return temporary_namespaces_;
}

bool ClientPolicyController::IsSupportedByDownload(
    const std::string& name_space) const {
  return GetPolicy(name_space).is_supported_by_download;
}

bool ClientPolicyController::IsPersistent(const std::string& name_space) const {
  return GetPolicy(name_space).lifetime_type == LifetimeType::PERSISTENT;
}

const std::vector<std::string>&
ClientPolicyController::GetPersistentNamespaces() const {
  return persistent_namespaces_;
}

bool ClientPolicyController::IsRestrictedToTabFromClientId(
    const std::string& name_space) const {
  return GetPolicy(name_space).is_restricted_to_tab_from_client_id;
}

bool ClientPolicyController::RequiresSpecificUserSettings(
    const std::string& name_space) const {
  return GetPolicy(name_space).requires_specific_user_settings;
}

bool ClientPolicyController::IsSuggested(const std::string& name_space) const {
  return GetPolicy(name_space).is_suggested;
}

bool ClientPolicyController::AllowsConversionToBackgroundFileDownload(
    const std::string& name_space) const {
  return GetPolicy(name_space).allows_conversion_to_background_file_download;
}

}  // namespace offline_pages
