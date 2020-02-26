// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_BROWSER_POLICY_CONNECTOR_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_BROWSER_POLICY_CONNECTOR_IOS_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/policy/core/browser/browser_policy_connector.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {
class ConfigurationPolicyProvider;
}  // namespace policy

// Extends BrowserPolicyConnector with the setup for iOS builds.
class BrowserPolicyConnectorIOS : public policy::BrowserPolicyConnector {
 public:
  BrowserPolicyConnectorIOS(
      const policy::HandlerListFactory& handler_list_factory);

  ~BrowserPolicyConnectorIOS() override;

  // Returns the platform provider used by this BrowserPolicyConnectorIOS. Can
  // be overridden for testing via
  // BrowserPolicyConnectorBase::SetPolicyProviderForTesting().
  policy::ConfigurationPolicyProvider* GetPlatformProvider();

  // BrowserPolicyConnector.
  void Init(PrefService* local_state,
            scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      override;
  bool IsEnterpriseManaged() const override;
  bool HasMachineLevelPolicies() override;

 protected:
  // BrowserPolicyConnectorBase.
  std::vector<std::unique_ptr<policy::ConfigurationPolicyProvider>>
  CreatePolicyProviders() override;

 private:
  std::unique_ptr<policy::ConfigurationPolicyProvider> CreatePlatformProvider();

  // Owned by base class.
  policy::ConfigurationPolicyProvider* platform_provider_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BrowserPolicyConnectorIOS);
};

#endif  // IOS_CHROME_BROWSER_POLICY_BROWSER_POLICY_CONNECTOR_IOS_H_
