// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SIGNIN_WEB_VIEW_DEVICE_ACCOUNTS_PROVIDER_IMPL_H_
#define IOS_WEB_VIEW_INTERNAL_SIGNIN_WEB_VIEW_DEVICE_ACCOUNTS_PROVIDER_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/signin/public/identity_manager/ios/device_accounts_provider.h"

class IOSWebViewSigninClient;

// Implementation of DeviceAccountsProvider.
class WebViewDeviceAccountsProviderImpl : public DeviceAccountsProvider {
 public:
  // |signin_client| used to fetch access tokens.
  explicit WebViewDeviceAccountsProviderImpl(
      IOSWebViewSigninClient* signin_client);
  ~WebViewDeviceAccountsProviderImpl() override;

  // ios::DeviceAccountsProvider
  void GetAccessToken(const std::string& gaia_id,
                      const std::string& client_id,
                      const std::set<std::string>& scopes,
                      AccessTokenCallback callback) override;
  std::vector<AccountInfo> GetAllAccounts() const override;
  AuthenticationErrorCategory GetAuthenticationErrorCategory(
      const std::string& gaia_id,
      NSError* error) const override;

 private:
  // Used to obtain access tokens in |GetAccessToken|.
  IOSWebViewSigninClient* const signin_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WebViewDeviceAccountsProviderImpl);
};

#endif  // IOS_WEB_VIEW_INTERNAL_SIGNIN_WEB_VIEW_DEVICE_ACCOUNTS_PROVIDER_IMPL_H_
