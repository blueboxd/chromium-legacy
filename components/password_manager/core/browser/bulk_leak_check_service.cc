// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/bulk_leak_check_service.h"

#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace password_manager {

BulkLeakCheckService::BulkLeakCheckService(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      leak_check_factory_(std::make_unique<LeakDetectionCheckFactoryImpl>()) {}

BulkLeakCheckService::~BulkLeakCheckService() = default;

void BulkLeakCheckService::CheckUsernamePasswordPairs(
    std::vector<password_manager::LeakCheckCredential> credentials) {}

void BulkLeakCheckService::Cancel() {
  bulk_leak_check_.reset();
}

size_t BulkLeakCheckService::GetPendingChecksCount() const {
  return bulk_leak_check_ ? bulk_leak_check_->GetPendingChecksCount() : 0;
}

void BulkLeakCheckService::Shutdown() {
  bulk_leak_check_.reset();
  url_loader_factory_.reset();
  identity_manager_ = nullptr;
}

}  // namespace password_manager
