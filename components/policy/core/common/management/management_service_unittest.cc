// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/management_service.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class TestManagementStatusProvider : public ManagementStatusProvider {
 public:
  TestManagementStatusProvider(EnterpriseManagementAuthority authority,
                               bool managed)
      : authority_(authority), managed_(managed) {}
  ~TestManagementStatusProvider() override = default;

  // Returns |true| if the service or component is managed.
  bool IsManaged() override { return managed_; }

  // Returns the authority responsible for the management.
  EnterpriseManagementAuthority GetAuthority() override { return authority_; }

 private:
  EnterpriseManagementAuthority authority_;
  bool managed_;
};

class TestManagementService : public ManagementService {
 public:
  void SetManagementStatusProviderForTesting(
      std::vector<std::unique_ptr<ManagementStatusProvider>> providers) {
    SetManagementStatusProvider(std::move(providers));
  }

 protected:
  // Initializes the management status providers.
  void InitManagementStatusProviders() override {}
};

// Tests that only the authorities that are actively managing are returned.
TEST(ManagementService, GetManagementAuthorities) {
  TestManagementService management_service;
  auto authorities = management_service.GetManagementAuthorities();
  EXPECT_TRUE(authorities.empty());
  std::vector<std::unique_ptr<ManagementStatusProvider>> providers;
  providers.emplace_back(std::make_unique<TestManagementStatusProvider>(
      EnterpriseManagementAuthority::CLOUD, true));
  providers.emplace_back(std::make_unique<TestManagementStatusProvider>(
      EnterpriseManagementAuthority::CLOUD_DOMAIN, false));
  providers.emplace_back(std::make_unique<TestManagementStatusProvider>(
      EnterpriseManagementAuthority::COMPUTER_LOCAL, false));
  providers.emplace_back(std::make_unique<TestManagementStatusProvider>(
      EnterpriseManagementAuthority::DOMAIN_LOCAL, true));
  management_service.SetManagementStatusProviderForTesting(
      std::move(providers));
  authorities = management_service.GetManagementAuthorities();
  EXPECT_EQ(authorities.size(), 2u);
  EXPECT_NE(authorities.find(EnterpriseManagementAuthority::CLOUD),
            authorities.end());
  EXPECT_NE(authorities.find(EnterpriseManagementAuthority::DOMAIN_LOCAL),
            authorities.end());
  ManagementAuthorityTrustworthiness trustworthyness =
      management_service.GetManagementAuthorityTrustworthiness();
  EXPECT_EQ(trustworthyness, ManagementAuthorityTrustworthiness::TRUSTED);
}

}  // namespace policy
