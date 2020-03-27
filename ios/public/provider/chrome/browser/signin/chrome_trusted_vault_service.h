// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_TRUSTED_VAULT_SERVICE_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_TRUSTED_VAULT_SERVICE_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"

@class ChromeIdentity;

namespace ios {

using TrustedVaultSharedKey = std::vector<uint8_t>;
using TrustedVaultSharedKeyList = std::vector<TrustedVaultSharedKey>;

// Abstract class to manage shared keys.
class ChromeTrustedVaultService {
 public:
  ChromeTrustedVaultService();
  virtual ~ChromeTrustedVaultService();

  // Asynchronously fetch the shared keys for |identity|
  // and returns them by calling |callback|.
  virtual void FetchKeys(
      ChromeIdentity* chrome_identity,
      base::OnceCallback<void(const TrustedVaultSharedKeyList&)> callback);

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeTrustedVaultService);
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_TRUSTED_VAULT_SERVICE_H_
