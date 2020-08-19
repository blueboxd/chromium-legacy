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
#include "components/sync/base/bind_to_task_runner.h"
#include "components/sync/trusted_vault/standalone_trusted_vault_backend.h"
#include "components/sync/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "components/sync/trusted_vault/trusted_vault_connection_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

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
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::FetchKeys, backend_,
                     account_info, BindToCurrentSequence(std::move(cb))));
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
  TriggerLazyInitializationIfNeeded();
  base::PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::MarkKeysAsStale, backend_,
                     account_info),
      std::move(cb));
}

void StandaloneTrustedVaultClient::GetIsRecoverabilityDegraded(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> cb) {
  // TODO(crbug.com/1081649): Implement logic.
  NOTIMPLEMENTED();
  std::move(cb).Run(is_recoverability_degraded_for_testing_);
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

  // TODO(crbug.com/1113597): populate TrustedVaultAccessTokenFetcher into
  // TrustedVaultConnectionImpl ctor.
  // TODO(crbug.com/1113598): populate URLLoaderFactory into
  // TrustedVaultConnectionImpl ctor.
  // TODO(crbug.com/1102340): allow setting custom TrustedVaultConnection for
  // testing.
  backend_ = base::MakeRefCounted<StandaloneTrustedVaultBackend>(
      file_path_,
      std::make_unique<TrustedVaultConnectionImpl>(
          /*url_loader_factory=*/nullptr, /*access_token_fetcher=*/nullptr));
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::ReadDataFromDisk,
                     backend_));
  // TODO(crbug.com/1113597): populate current syncing account to |backend_|
  // here and upon syncing account change.
}

bool StandaloneTrustedVaultClient::IsInitializationTriggeredForTesting() const {
  return backend_ != nullptr;
}

void StandaloneTrustedVaultClient::SetRecoverabilityDegradedForTesting() {
  is_recoverability_degraded_for_testing_ = true;
}

}  // namespace syncer
