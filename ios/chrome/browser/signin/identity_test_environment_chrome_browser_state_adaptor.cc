// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/identity_test_environment_chrome_browser_state_adaptor.h"

#include <utility>

#include "base/bind.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/test_signin_client.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/device_accounts_provider_impl.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/signin/signin_client_factory.h"

namespace {

std::unique_ptr<KeyedService> BuildTestSigninClient(web::BrowserState* state) {
  return std::make_unique<TestSigninClient>(
      ios::ChromeBrowserState::FromBrowserState(state)->GetPrefs());
}

}  // namespace

// static
std::unique_ptr<TestChromeBrowserState>
IdentityTestEnvironmentChromeBrowserStateAdaptor::
    CreateChromeBrowserStateForIdentityTestEnvironment() {
  return CreateChromeBrowserStateForIdentityTestEnvironment(
      TestChromeBrowserState::TestingFactories());
}

// static
std::unique_ptr<TestChromeBrowserState>
IdentityTestEnvironmentChromeBrowserStateAdaptor::
    CreateChromeBrowserStateForIdentityTestEnvironment(
        const TestChromeBrowserState::TestingFactories& input_factories) {
  TestChromeBrowserState::Builder builder;

  for (auto& input_factory : input_factories) {
    builder.AddTestingFactory(input_factory.first, input_factory.second);
  }

  return CreateChromeBrowserStateForIdentityTestEnvironment(builder);
}

// static
std::unique_ptr<TestChromeBrowserState>
IdentityTestEnvironmentChromeBrowserStateAdaptor::
    CreateChromeBrowserStateForIdentityTestEnvironment(
        TestChromeBrowserState::Builder& builder,
        bool use_ios_token_service_delegate) {
  for (auto& identity_factory :
       GetIdentityTestEnvironmentFactories(use_ios_token_service_delegate)) {
    builder.AddTestingFactory(identity_factory.first, identity_factory.second);
  }

  return builder.Build();
}

// static
void IdentityTestEnvironmentChromeBrowserStateAdaptor::
    SetIdentityTestEnvironmentFactoriesOnBrowserContext(
        TestChromeBrowserState* chrome_browser_state) {
  for (const auto& factory_pair : GetIdentityTestEnvironmentFactories(
           /*use_ios_token_service_delegate=*/false)) {
    factory_pair.first->SetTestingFactory(chrome_browser_state,
                                          factory_pair.second);
  }
}

// static
void IdentityTestEnvironmentChromeBrowserStateAdaptor::
    AppendIdentityTestEnvironmentFactories(
        TestChromeBrowserState::TestingFactories* factories_to_append_to) {
  TestChromeBrowserState::TestingFactories identity_factories =
      GetIdentityTestEnvironmentFactories(
          /*use_ios_token_service_delegate=*/false);
  factories_to_append_to->insert(factories_to_append_to->end(),
                                 identity_factories.begin(),
                                 identity_factories.end());
}

// static
std::unique_ptr<KeyedService>
IdentityTestEnvironmentChromeBrowserStateAdaptor::BuildIdentityManagerForTests(
    web::BrowserState* browser_state) {
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(browser_state);

  return identity::IdentityTestEnvironment::BuildIdentityManagerForTests(
      SigninClientFactory::GetForBrowserState(chrome_browser_state),
      chrome_browser_state->GetPrefs(), base::FilePath(),
      signin::AccountConsistencyMethod::kMirror);
}

// static
std::unique_ptr<KeyedService> IdentityTestEnvironmentChromeBrowserStateAdaptor::
    BuildIdentityManagerForTestWithIOSDelegate(
        web::BrowserState* browser_state) {
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(browser_state);

  identity::IdentityTestEnvironment::ExtraParams extra_params;
  extra_params.token_service_provider =
      std::make_unique<DeviceAccountsProviderImpl>();

  return identity::IdentityTestEnvironment::BuildIdentityManagerForTests(
      SigninClientFactory::GetForBrowserState(chrome_browser_state),
      chrome_browser_state->GetPrefs(), base::FilePath(),
      signin::AccountConsistencyMethod::kMirror, std::move(extra_params));
}

// static
TestChromeBrowserState::TestingFactories
IdentityTestEnvironmentChromeBrowserStateAdaptor::
    GetIdentityTestEnvironmentFactories(bool use_ios_token_service_delegate) {
  return {{SigninClientFactory::GetInstance(),
           base::BindRepeating(&BuildTestSigninClient)},
          {IdentityManagerFactory::GetInstance(),
           base::BindRepeating(use_ios_token_service_delegate
                                   ? &BuildIdentityManagerForTestWithIOSDelegate
                                   : &BuildIdentityManagerForTests)}};
}

IdentityTestEnvironmentChromeBrowserStateAdaptor::
    IdentityTestEnvironmentChromeBrowserStateAdaptor(
        ios::ChromeBrowserState* chrome_browser_state)
    : identity_test_env_(
          IdentityManagerFactory::GetForBrowserState(chrome_browser_state)) {}
