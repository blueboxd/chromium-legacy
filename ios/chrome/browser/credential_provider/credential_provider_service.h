// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_SERVICE_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_SERVICE_H_

#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_store.h"

// A browser-context keyed service that is used to keep the Credential Provider
// Extension data up to date.
class CredentialProviderService : public KeyedService {
 public:
  // Initializes the service.
  CredentialProviderService(
      scoped_refptr<password_manager::PasswordStore> password_store);
  ~CredentialProviderService() override;

  // KeyedService:
  void Shutdown() override;

 private:
  friend class CredentialProviderServiceTest;

  // The interface for getting and manipulating a user's saved passwords.
  scoped_refptr<password_manager::PasswordStore> password_store_;

  DISALLOW_COPY_AND_ASSIGN(CredentialProviderService);
};

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_SERVICE_H_
