// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_AUTHENTICATED_LEAK_CHECK_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_AUTHENTICATED_LEAK_CHECK_H_

#include "base/memory/scoped_refptr.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace password_manager {

class LeakDetectionDelegateInterface;

// Performs a leak-check for {username, password} for Chrome signed-in users.
class AuthenticatedLeakCheck : public LeakDetectionCheck {
 public:
  AuthenticatedLeakCheck(
      LeakDetectionDelegateInterface* delegate,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~AuthenticatedLeakCheck() override;

  void Start(const GURL& url,
             base::StringPiece16 username,
             base::StringPiece16 password) override;

 private:
  // Delegate for the instance. Should outlive |this|.
  LeakDetectionDelegateInterface* delegate_;
  // Identity manager for the profile.
  signin::IdentityManager* identity_manager_;
  // URL loader factory required for the network request to the identity
  // endpoint.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_AUTHENTICATED_LEAK_CHECK_H_
