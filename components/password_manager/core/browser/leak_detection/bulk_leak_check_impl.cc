// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_impl.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace password_manager {

// Holds all necessary payload for the request to the server for one credential.
struct BulkLeakCheckImpl::CredentialHolder {
  explicit CredentialHolder(LeakCheckCredential c) : credential(std::move(c)) {}
  ~CredentialHolder() = default;

  CredentialHolder(const CredentialHolder&) = delete;
  CredentialHolder& operator=(const CredentialHolder&) = delete;

  // Client supplied credential to be checked.
  LeakCheckCredential credential;

  // Payload to be sent to the server.
  LookupSingleLeakPayload payload;
};

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
      url_loader_factory_(std::move(url_loader_factory)),
      encryption_key_(CreateNewKey()),
      payload_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {
  DCHECK(delegate_);
  DCHECK(identity_manager_);
  DCHECK(url_loader_factory_);
  DCHECK(!encryption_key_.empty());
}

BulkLeakCheckImpl::~BulkLeakCheckImpl() = default;

void BulkLeakCheckImpl::CheckCredentials(
    std::vector<LeakCheckCredential> credentials) {
  for (auto& c : credentials) {
    waiting_encryption_.push_back(
        std::make_unique<CredentialHolder>(std::move(c)));
    const LeakCheckCredential& credential =
        waiting_encryption_.back()->credential;
    PrepareSingleLeakRequestData(
        payload_task_runner_.get(), encryption_key_,
        base::UTF16ToUTF8(credential.username()),
        base::UTF16ToUTF8(credential.password()),
        base::BindOnce(&BulkLeakCheckImpl::OnPayloadReady,
                       weak_ptr_factory_.GetWeakPtr(),
                       waiting_encryption_.back().get()));
  }
}

size_t BulkLeakCheckImpl::GetPendingChecksCount() const {
  return 0;
}

void BulkLeakCheckImpl::OnPayloadReady(CredentialHolder* holder,
                                       LookupSingleLeakPayload payload) {
  // TODO(crbug.com/1049185): request an access token.
}

}  // namespace password_manager