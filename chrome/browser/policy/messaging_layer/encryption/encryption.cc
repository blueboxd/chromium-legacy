// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/hash/hash.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "chrome/browser/policy/messaging_layer/encryption/encryption.h"

namespace reporting {

EncryptorBase::Handle::Handle(scoped_refptr<EncryptorBase> encryptor)
    : encryptor_(encryptor) {}

EncryptorBase::Handle::~Handle() = default;

EncryptorBase::EncryptorBase()
    : asymmetric_key_sequenced_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::TaskPriority::BEST_EFFORT, base::MayBlock()})) {
  DETACH_FROM_SEQUENCE(asymmetric_key_sequence_checker_);
}

EncryptorBase::~EncryptorBase() = default;

void EncryptorBase::UpdateAsymmetricKey(
    base::StringPiece new_key,
    base::OnceCallback<void(Status)> response_cb) {
  if (new_key.empty()) {
    std::move(response_cb)
        .Run(Status(error::INVALID_ARGUMENT, "Provided key is empty"));
    return;
  }

  // Schedule key update on the sequenced task runner.
  asymmetric_key_sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::StringPiece new_key,
                        scoped_refptr<EncryptorBase> encryptor) {
                       encryptor->asymmetric_key_ = std::string(new_key);
                     },
                     new_key, base::WrapRefCounted(this)));

  // Response OK not waiting for the update.
  std::move(response_cb).Run(Status::StatusOK());
}

void EncryptorBase::RetrieveAsymmetricKey(
    base::OnceCallback<void(StatusOr<std::string>)> cb) {
  // Schedule key retrueval on the sequenced task runner.
  asymmetric_key_sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceCallback<void(StatusOr<std::string>)> cb,
             scoped_refptr<EncryptorBase> encryptor) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(
                encryptor->asymmetric_key_sequence_checker_);
            StatusOr<std::string> response;
            // Schedule response on regular thread pool.
            base::ThreadPool::PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](base::OnceCallback<void(StatusOr<std::string>)> cb,
                       StatusOr<std::string> response) {
                      std::move(cb).Run(response);
                    },
                    std::move(cb),
                    !encryptor->asymmetric_key_.has_value()
                        ? StatusOr<std::string>(Status(
                              error::NOT_FOUND, "Asymmetric key not set"))
                        : encryptor->asymmetric_key_.value()));
          },
          std::move(cb), base::WrapRefCounted(this)));
}

void EncryptorBase::EncryptKey(
    base::StringPiece symmetric_key,
    base::OnceCallback<void(StatusOr<std::pair<uint32_t, std::string>>)> cb) {
  RetrieveAsymmetricKey(base::BindOnce(
      [](base::StringPiece symmetric_key,
         scoped_refptr<EncryptorBase> encryptor,
         base::OnceCallback<void(StatusOr<std::pair<uint32_t, std::string>>)>
             cb,
         StatusOr<std::string> asymmetric_key_result) {
        if (!asymmetric_key_result.ok()) {
          std::move(cb).Run(asymmetric_key_result.status());
          return;
        }
        const auto& asymmetric_key = asymmetric_key_result.ValueOrDie();
        std::move(cb).Run(std::make_pair(
            base::PersistentHash(asymmetric_key),
            encryptor->EncryptSymmetricKey(symmetric_key, asymmetric_key)));
      },
      std::string(symmetric_key), base::WrapRefCounted(this), std::move(cb)));
}

}  // namespace reporting
