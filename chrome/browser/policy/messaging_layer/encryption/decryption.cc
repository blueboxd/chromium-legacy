// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/hash/hash.h"
#include "base/strings/strcat.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "chrome/browser/policy/messaging_layer/encryption/decryption.h"

namespace reporting {

DecryptorBase::Handle::Handle(scoped_refptr<DecryptorBase> decryptor)
    : decryptor_(decryptor) {}

DecryptorBase::Handle::~Handle() = default;

DecryptorBase::DecryptorBase()
    : keys_sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock()})) {
  DETACH_FROM_SEQUENCE(keys_sequence_checker_);
}

DecryptorBase::~DecryptorBase() = default;

void DecryptorBase::RecordKeyPair(base::StringPiece private_key,
                                  base::StringPiece public_key,
                                  base::OnceCallback<void(Status)> cb) {
  // Schedule key recording on the sequenced task runner.
  keys_sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::string public_key, KeyInfo key_info,
             base::OnceCallback<void(Status)> cb,
             scoped_refptr<DecryptorBase> decryptor) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(decryptor->keys_sequence_checker_);
            Status result;
            if (!decryptor->keys_
                     .emplace(base::PersistentHash(public_key.data(),
                                                   public_key.size()),
                              key_info)
                     .second) {
              result = Status(error::ALREADY_EXISTS,
                              base::StrCat({"Public key='", public_key,
                                            "' already recorded"}));
            }
            // Schedule response on a generic thread pool.
            base::ThreadPool::PostTask(
                FROM_HERE,
                base::BindOnce([](base::OnceCallback<void(Status)> cb,
                                  Status result) { std::move(cb).Run(result); },
                               std::move(cb), result));
          },
          std::string(public_key),
          KeyInfo{.private_key = std::string(private_key),
                  .time_stamp = base::Time::Now()},
          std::move(cb), base::WrapRefCounted(this)));
}

void DecryptorBase::RetrieveMatchingPrivateKey(
    uint32_t public_key_id,
    base::OnceCallback<void(StatusOr<std::string>)> cb) {
  // Schedule key retrieval on the sequenced task runner.
  keys_sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](uint32_t public_key_id,
             base::OnceCallback<void(StatusOr<std::string>)> cb,
             scoped_refptr<DecryptorBase> decryptor) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(decryptor->keys_sequence_checker_);
            auto key_info_it = decryptor->keys_.find(public_key_id);
            // Schedule response on a generic thread pool.
            base::ThreadPool::PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](base::OnceCallback<void(StatusOr<std::string>)> cb,
                       StatusOr<std::string> result) {
                      std::move(cb).Run(result);
                    },
                    std::move(cb),
                    key_info_it == decryptor->keys_.end()
                        ? StatusOr<std::string>(Status(
                              error::NOT_FOUND, "Matching key not found"))
                        : key_info_it->second.private_key));
          },
          public_key_id, std::move(cb), base::WrapRefCounted(this)));
}

}  // namespace reporting
