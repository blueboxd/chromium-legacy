// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/standalone_trusted_vault_client.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/trusted_vault/standalone_trusted_vault_backend.h"

namespace syncer {

namespace {

constexpr base::TaskTraits kBackendTaskTraits = {
    base::MayBlock(), base::TaskPriority::USER_VISIBLE,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

}  // namespace

StandaloneTrustedVaultClient::StandaloneTrustedVaultClient(
    const base::FilePath& file_path)
    : file_path_(file_path),
      backend_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(kBackendTaskTraits)) {}

StandaloneTrustedVaultClient::~StandaloneTrustedVaultClient() = default;

std::unique_ptr<StandaloneTrustedVaultClient::Subscription>
StandaloneTrustedVaultClient::AddKeysChangedObserver(
    const base::RepeatingClosure& cb) {
  return observer_list_.Add(cb);
}

void StandaloneTrustedVaultClient::FetchKeys(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)> cb) {
  TriggerLazyInitializationIfNeeded();
  base::PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::FetchKeys, backend_,
                     account_info),
      std::move(cb));
}

void StandaloneTrustedVaultClient::StoreKeys(
    const std::string& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {
  TriggerLazyInitializationIfNeeded();
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&StandaloneTrustedVaultBackend::StoreKeys,
                                backend_, gaia_id, keys, last_key_version));
  observer_list_.Notify();
}

void StandaloneTrustedVaultClient::RemoveAllStoredKeys() {
  TriggerLazyInitializationIfNeeded();
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::RemoveAllStoredKeys,
                     backend_));
  observer_list_.Notify();
}

void StandaloneTrustedVaultClient::MarkKeysAsStale(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> cb) {
  // Not really supported and not useful for this particular implementation.
  std::move(cb).Run(false);
}

void StandaloneTrustedVaultClient::WaitForFlushForTesting(
    base::OnceClosure cb) const {
  backend_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                         std::move(cb));
}

void StandaloneTrustedVaultClient::TriggerLazyInitializationIfNeeded() {
  if (backend_) {
    return;
  }

  backend_ = base::MakeRefCounted<StandaloneTrustedVaultBackend>(file_path_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::ReadDataFromDisk,
                     backend_));
}

bool StandaloneTrustedVaultClient::IsInitializationTriggeredForTesting() const {
  return backend_ != nullptr;
}

}  // namespace syncer
