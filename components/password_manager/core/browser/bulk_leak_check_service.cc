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
    std::vector<password_manager::LeakCheckCredential> credentials) {
  if (bulk_leak_check_) {
    DCHECK_EQ(State::kRunning, state_);
    // The check is already running. Append the credentials to the list.
    bulk_leak_check_->CheckCredentials(std::move(credentials));
    // Notify the observers because the number of pending credentials changed.
    NotifyStateChanged();
    return;
  }

  bulk_leak_check_ = leak_check_factory_->TryCreateBulkLeakCheck(
      this, identity_manager_, url_loader_factory_);
  if (!bulk_leak_check_) {
    // The factory may have called OnError() so the service contains the correct
    // error state.
    return;
  }
  // The state is 'running now'. CheckCredentials() can trigger OnError() that
  // will change it to something else.
  state_ = State::kRunning;
  bulk_leak_check_->CheckCredentials(std::move(credentials));
  // Notify the observers after the call because the number of pending
  // credentials after CheckCredentials.
  NotifyStateChanged();
}

void BulkLeakCheckService::Cancel() {
  if (!bulk_leak_check_) {
    DCHECK_NE(State::kRunning, state_);
    return;
  }
  state_ = State::kIdle;
  bulk_leak_check_.reset();
  NotifyStateChanged();
}

size_t BulkLeakCheckService::GetPendingChecksCount() const {
  return bulk_leak_check_ ? bulk_leak_check_->GetPendingChecksCount() : 0;
}

void BulkLeakCheckService::Shutdown() {
  observers_.Clear();
  bulk_leak_check_.reset();
  url_loader_factory_.reset();
  identity_manager_ = nullptr;
}

void BulkLeakCheckService::OnFinishedCredential(LeakCheckCredential credential,
                                                IsLeaked is_leaked) {
  // (1) Make sure that the state of the service is correct.
  // (2) Notify about the leak if necessary.
  // (3) Notify about new state. The clients may assume that if the state is
  // idle then there won't be calls to OnLeakFound.
  if (!GetPendingChecksCount()) {
    state_ = State::kIdle;
    bulk_leak_check_.reset();
  }
  if (is_leaked) {
    for (Observer& obs : observers_)
      obs.OnLeakFound(credential);
  }
  if (state_ == State::kIdle)
    NotifyStateChanged();
}

void BulkLeakCheckService::OnError(LeakDetectionError error) {
  switch (error) {
    case LeakDetectionError::kNotSignIn:
      state_ = State::kSignedOut;
      break;
    case LeakDetectionError::kTokenRequestFailure:
      state_ = State::kTokenRequestFailure;
      break;
    case LeakDetectionError::kHashingFailure:
      state_ = State::kHashingFailure;
      break;
    case LeakDetectionError::kInvalidServerResponse:
      state_ = State::kServiceError;
      break;
  }
  bulk_leak_check_.reset();
  NotifyStateChanged();
}

void BulkLeakCheckService::NotifyStateChanged() {
  size_t pending_count = GetPendingChecksCount();
  for (Observer& obs : observers_)
    obs.OnStateChanged(state_, pending_count);
}

}  // namespace password_manager
