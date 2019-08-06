// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/authenticated_leak_check.h"

#include <utility>

#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace password_manager {

AuthenticatedLeakCheck::AuthenticatedLeakCheck(
    LeakDetectionDelegateInterface* delegate,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : delegate_(delegate),
      identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)) {}

AuthenticatedLeakCheck::~AuthenticatedLeakCheck() = default;

void AuthenticatedLeakCheck::Start(const GURL& url,
                                   base::StringPiece16 username,
                                   base::StringPiece16 password) {
  // TODO(crbug.com/986298): get the access token here.
  std::ignore = delegate_;
  std::ignore = identity_manager_;
  // TODO(crbug.com/986298): Initiate the network request.
  std::ignore = url_loader_factory_;
}

}  // namespace password_manager
