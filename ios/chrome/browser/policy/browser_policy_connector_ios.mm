// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/browser_policy_connector_ios.h"

#include <stdint.h>

#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_loader_ios.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using policy::AsyncPolicyLoader;
using policy::AsyncPolicyProvider;
using policy::BrowserPolicyConnectorBase;
using policy::BrowserPolicyConnector;
using policy::ConfigurationPolicyProvider;
using policy::HandlerListFactory;
using policy::PolicyLoaderIOS;

BrowserPolicyConnectorIOS::BrowserPolicyConnectorIOS(
    const HandlerListFactory& handler_list_factory)
    : BrowserPolicyConnector(handler_list_factory) {}

BrowserPolicyConnectorIOS::~BrowserPolicyConnectorIOS() {}

ConfigurationPolicyProvider* BrowserPolicyConnectorIOS::GetPlatformProvider() {
  ConfigurationPolicyProvider* provider =
      BrowserPolicyConnectorBase::GetPolicyProviderForTesting();
  return provider ? provider : platform_provider_;
}

void BrowserPolicyConnectorIOS::Init(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  InitInternal(local_state, /*device_management_service=*/nullptr);
}

bool BrowserPolicyConnectorIOS::IsEnterpriseManaged() const {
  NOTREACHED() << "This method is only defined for Chrome OS";
  return false;
}

bool BrowserPolicyConnectorIOS::HasMachineLevelPolicies() {
  return ProviderHasPolicies(GetPlatformProvider());
}

std::vector<std::unique_ptr<policy::ConfigurationPolicyProvider>>
BrowserPolicyConnectorIOS::CreatePolicyProviders() {
  auto providers = BrowserPolicyConnector::CreatePolicyProviders();
  std::unique_ptr<ConfigurationPolicyProvider> platform_provider =
      CreatePlatformProvider();
  if (platform_provider) {
    DCHECK(!platform_provider_) << "CreatePolicyProviders was called twice.";
    platform_provider_ = platform_provider.get();
    // PlatformProvider should be before all other providers (highest priority).
    providers.insert(providers.begin(), std::move(platform_provider));
  }
  return providers;
}

std::unique_ptr<ConfigurationPolicyProvider>
BrowserPolicyConnectorIOS::CreatePlatformProvider() {
  auto loader = std::make_unique<PolicyLoaderIOS>(
      GetSchemaRegistry(),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));

  return std::make_unique<AsyncPolicyProvider>(GetSchemaRegistry(),
                                               std::move(loader));
}
