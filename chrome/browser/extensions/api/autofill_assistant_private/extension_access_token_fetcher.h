// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_ASSISTANT_PRIVATE_EXTENSION_ACCESS_TOKEN_FETCHER_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_ASSISTANT_PRIVATE_EXTENSION_ACCESS_TOKEN_FETCHER_H_

#include "components/autofill_assistant/browser/access_token_fetcher.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace extensions {

// A class that fetches tokens as needed for requests to Autofill Assistant
// backends. This is an implementation provided to a service in the AA
// component.
class ExtensionAccessTokenFetcher
    : public autofill_assistant::AccessTokenFetcher {
 public:
  explicit ExtensionAccessTokenFetcher(
      signin::IdentityManager* identity_manager);
  ~ExtensionAccessTokenFetcher() override;

  // AccessTokenFetcher:
  void FetchAccessToken(
      base::OnceCallback<void(bool, const std::string&)> callback) override;
  void InvalidateAccessToken(const std::string& access_token) override;

 private:
  void OnCompleted(GoogleServiceAuthError error,
                   signin::AccessTokenInfo access_token_info);

  signin::IdentityManager* identity_manager_;
  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;
  base::OnceCallback<void(bool, const std::string&)> callback_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_ASSISTANT_PRIVATE_EXTENSION_ACCESS_TOKEN_FETCHER_H_
