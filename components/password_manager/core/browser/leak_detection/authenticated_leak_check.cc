// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/authenticated_leak_check.h"

namespace password_manager {

AuthenticatedLeakCheck::AuthenticatedLeakCheck(
    LeakDetectionDelegateInterface* delegate,
    signin::IdentityManager* identity_manager)
    : delegate_(delegate), identity_manager_(identity_manager) {}

AuthenticatedLeakCheck::~AuthenticatedLeakCheck() = default;

void AuthenticatedLeakCheck::Start(const GURL& url,
                                   base::StringPiece16 username,
                                   base::StringPiece16 password) {
  // TODO(crbug.com/986298): get the access token here.
  std::ignore = delegate_;
  std::ignore = identity_manager_;
}

}  // namespace password_manager
