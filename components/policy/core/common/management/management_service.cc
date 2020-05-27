// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/management_service.h"

namespace policy {

namespace {

ManagementAuthorityTrustworthiness GetTrustworthiness(
    const base::flat_set<EnterpriseManagementAuthority> authorities) {
  if (authorities.find(EnterpriseManagementAuthority::CLOUD_DOMAIN) !=
      authorities.end())
    return ManagementAuthorityTrustworthiness::FULLY_TRUSTED;
  if (authorities.find(EnterpriseManagementAuthority::CLOUD) !=
      authorities.end())
    return ManagementAuthorityTrustworthiness::TRUSTED;
  if (authorities.find(EnterpriseManagementAuthority::DOMAIN_LOCAL) !=
      authorities.end())
    return ManagementAuthorityTrustworthiness::TRUSTED;
  if (authorities.find(EnterpriseManagementAuthority::COMPUTER_LOCAL) !=
      authorities.end())
    return ManagementAuthorityTrustworthiness::LOW;
  return ManagementAuthorityTrustworthiness::NONE;
}

}  // namespace

ManagementStatusProvider::~ManagementStatusProvider() = default;

ManagementService::ManagementService() = default;
ManagementService::~ManagementService() = default;

base::flat_set<EnterpriseManagementAuthority>
ManagementService::GetManagementAuthorities() {
  base::flat_set<EnterpriseManagementAuthority> result;
  for (const auto& delegate : management_status_providers_) {
    if (delegate->IsManaged())
      result.insert(delegate->GetAuthority());
  }
  return result;
}

ManagementAuthorityTrustworthiness
ManagementService::GetManagementAuthorityTrustworthiness() {
  return GetTrustworthiness(GetManagementAuthorities());
}

void ManagementService::SetManagementStatusProvider(
    std::vector<std::unique_ptr<ManagementStatusProvider>> providers) {
  management_status_providers_ = std::move(providers);
}

}  // namespace policy
