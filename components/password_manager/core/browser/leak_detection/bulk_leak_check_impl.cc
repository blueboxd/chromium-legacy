// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_impl.h"

#include <utility>

#include "base/logging.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace password_manager {

LeakCheckCredential::LeakCheckCredential(base::string16 username,
                                         base::string16 password)
    : username_(std::move(username)), password_(std::move(password)) {}

LeakCheckCredential::LeakCheckCredential(LeakCheckCredential&&) = default;

LeakCheckCredential& LeakCheckCredential::operator=(LeakCheckCredential&&) =
    default;

LeakCheckCredential::~LeakCheckCredential() = default;

BulkLeakCheckImpl::BulkLeakCheckImpl(
    BulkLeakCheckDelegateInterface* delegate,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : delegate_(delegate),
      identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)) {
  DCHECK(delegate_);
  DCHECK(identity_manager_);
  DCHECK(url_loader_factory_);
}

BulkLeakCheckImpl::~BulkLeakCheckImpl() = default;

void BulkLeakCheckImpl::CheckCredentials(
    std::vector<LeakCheckCredential> credentials) {
  for (auto& c : credentials)
    delegate_->OnFinishedCredential(std::move(c), IsLeaked(false));
}

size_t BulkLeakCheckImpl::GetPendingChecksCount() const {
  return 0;
}

}  // namespace password_manager