// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_BULK_LEAK_CHECK_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_BULK_LEAK_CHECK_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace password_manager {

class BulkLeakCheckDelegateInterface;

// Implementation of the bulk leak check.
// Every credential in the list is processed consequitively:
// - prepare payload for the request.
// - get the access token.
// - make a network request.
// - decrypt the response.
// Encryption/decryption part is expensive and, therefore, done only on one
// background sequence.
class BulkLeakCheckImpl : public BulkLeakCheck {
 public:
  BulkLeakCheckImpl(
      BulkLeakCheckDelegateInterface* delegate,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~BulkLeakCheckImpl() override;

  // BulkLeakCheck:
  void CheckCredentials(std::vector<LeakCheckCredential> credentials) override;
  size_t GetPendingChecksCount() const override;

 private:
  // Delegate for the instance. Should outlive |this|.
  BulkLeakCheckDelegateInterface* const delegate_;

  // Identity manager for the profile.
  signin::IdentityManager* const identity_manager_;

  // URL loader factory required for the network request to the identity
  // endpoint.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_BULK_LEAK_CHECK_IMPL_H_
