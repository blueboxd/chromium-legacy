// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/bulk_leak_check_service_adapter.h"
#include "base/logging.h"

namespace password_manager {
BulkLeakCheckServiceAdapter::BulkLeakCheckServiceAdapter(
    SavedPasswordsPresenter* presenter,
    BulkLeakCheckService* service)
    : presenter_(presenter), service_(service) {
  DCHECK(presenter_);
  DCHECK(service_);
  presenter_->AddObserver(this);
}

BulkLeakCheckServiceAdapter::~BulkLeakCheckServiceAdapter() {
  presenter_->RemoveObserver(this);
}

void BulkLeakCheckServiceAdapter::StartBulkLeakCheck() {
  // TODO(crbug.com/1047726): Implement.
  NOTIMPLEMENTED();
}

void BulkLeakCheckServiceAdapter::StopBulkLeakCheck() {
  // TODO(crbug.com/1047726): Implement.
  NOTIMPLEMENTED();
}

BulkLeakCheckService::State BulkLeakCheckServiceAdapter::GetBulkLeakCheckState()
    const {
  return service_->state();
}

size_t BulkLeakCheckServiceAdapter::GetPendingChecksCount() const {
  return service_->GetPendingChecksCount();
}

void BulkLeakCheckServiceAdapter::OnEdited(const autofill::PasswordForm& form) {
  // TODO(crbug.com/1047726): Implement.
  NOTIMPLEMENTED();
}

}  // namespace password_manager
