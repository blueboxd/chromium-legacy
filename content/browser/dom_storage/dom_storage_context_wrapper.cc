// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_context_wrapper.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/services/storage/dom_storage/local_storage_impl.h"
#include "components/services/storage/dom_storage/session_storage_impl.h"
#include "components/services/storage/public/cpp/constants.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace content {
namespace {

const char kSessionStorageDirectory[] = "Session Storage";

void AdaptSessionStorageUsageInfo(
    DOMStorageContext::GetSessionStorageUsageCallback callback,
    std::vector<storage::mojom::SessionStorageUsageInfoPtr> usage) {
  std::vector<SessionStorageUsageInfo> result;
  result.reserve(usage.size());
  for (const auto& entry : usage) {
    SessionStorageUsageInfo info;
    info.origin = entry->origin.GetURL();
    info.namespace_id = entry->namespace_id;
    result.push_back(std::move(info));
  }
  std::move(callback).Run(result);
}

void AdaptLocalStorageUsageInfo(
    DOMStorageContext::GetLocalStorageUsageCallback callback,
    std::vector<storage::mojom::LocalStorageUsageInfoPtr> usage) {
  std::vector<StorageUsageInfo> result;
  result.reserve(usage.size());
  for (const auto& info : usage) {
    result.emplace_back(info->origin, info->size_in_bytes,
                        info->last_modified_time);
  }
  std::move(callback).Run(result);
}

void HandleSessionStorageBindingResult(
    const std::string& namespace_id,
    mojo::ReportBadMessageCallback bad_message_callback,
    bool success) {
  if (success)
    return;

  std::move(bad_message_callback)
      .Run("Request for unknown namespace: " + namespace_id);
}

}  // namespace

class DOMStorageContextWrapper::StoragePolicyObserver
    : public storage::SpecialStoragePolicy::Observer {
 public:
  explicit StoragePolicyObserver(
      scoped_refptr<storage::SpecialStoragePolicy> storage_policy,
      scoped_refptr<DOMStorageContextWrapper> context_wrapper)
      : storage_policy_(std::move(storage_policy)),
        context_wrapper_(std::move(context_wrapper)) {
    storage_policy_->AddObserver(this);
  }

  StoragePolicyObserver(const StoragePolicyObserver&) = delete;
  StoragePolicyObserver& operator=(const StoragePolicyObserver&) = delete;

  ~StoragePolicyObserver() override {
    DCHECK(!context_wrapper_);
    storage_policy_->RemoveObserver(this);
  }

  void DidShutdownContextWrapper() { context_wrapper_.reset(); }

 private:
  // storage::SpecialStoragePolicy::Observer:
  void OnPolicyChanged() override {
    if (!context_wrapper_)
      return;

    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&DOMStorageContextWrapper::OnStoragePolicyChanged,
                       context_wrapper_));
  }

  const scoped_refptr<storage::SpecialStoragePolicy> storage_policy_;
  scoped_refptr<DOMStorageContextWrapper> context_wrapper_;
};

scoped_refptr<DOMStorageContextWrapper> DOMStorageContextWrapper::Create(
    const base::FilePath& profile_path,
    const base::FilePath& local_partition_path,
    storage::SpecialStoragePolicy* special_storage_policy) {
  base::FilePath data_path;
  const bool is_profile_persistent = !profile_path.empty();
  if (is_profile_persistent)
    data_path = profile_path.Append(local_partition_path);

  auto mojo_task_runner =
      base::CreateSingleThreadTaskRunner({BrowserThread::IO});

  // TODO(https://crbug.com/1000959): These should be bound in an instance of
  // the Storage Service. For now we bind them alone on the IO thread because
  // that's where the implementation has effectively lived for some time.
  mojo::Remote<storage::mojom::SessionStorageControl> session_storage_control;
  mojo::Remote<storage::mojom::LocalStorageControl> local_storage_control;
  mojo_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const base::FilePath& storage_root, bool is_profile_persistent,
             mojo::PendingReceiver<storage::mojom::SessionStorageControl>
                 session_storage_receiver,
             mojo::PendingReceiver<storage::mojom::LocalStorageControl>
                 local_storage_receiver) {
            // Deletes itself on shutdown completion.
            new storage::SessionStorageImpl(
                storage_root,
                base::CreateSequencedTaskRunner(
                    {base::ThreadPool(), base::MayBlock(),
                     base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
                base::CreateSingleThreadTaskRunner({BrowserThread::IO}),
#if defined(OS_ANDROID)
                // On Android there is no support for session storage restoring,
                // and since the restoring code is responsible for database
                // cleanup, we must manually delete the old database here before
                // we open it.
                storage::SessionStorageImpl::BackingMode::kClearDiskStateOnOpen,
#else
                is_profile_persistent
                    ? storage::SessionStorageImpl::BackingMode ::
                          kRestoreDiskState
                    : storage::SessionStorageImpl::BackingMode::kNoDisk,
#endif
                std::string(kSessionStorageDirectory),
                std::move(session_storage_receiver));
            new storage::LocalStorageImpl(
                storage_root,
                base::CreateSingleThreadTaskRunner({BrowserThread::IO}),
                base::CreateSequencedTaskRunner(
                    {base::ThreadPool(), base::MayBlock(),
                     base::TaskPriority::USER_BLOCKING,
                     base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
                std::move(local_storage_receiver));
          },
          data_path, is_profile_persistent,
          session_storage_control.BindNewPipeAndPassReceiver(),
          local_storage_control.BindNewPipeAndPassReceiver()));

  auto wrapper = base::WrapRefCounted(new DOMStorageContextWrapper(
      std::move(session_storage_control), std::move(local_storage_control),
      special_storage_policy));

  if (special_storage_policy) {
    // If there's a SpecialStoragePolicy, ensure the wrapper is observing it on
    // the IO thread and query the initial set of in-use origins ASAP.
    wrapper->storage_policy_observer_ =
        base::SequenceBound<StoragePolicyObserver>(
            base::CreateSequencedTaskRunner(BrowserThread::IO),
            base::WrapRefCounted(special_storage_policy), wrapper);

    wrapper->local_storage_control_->GetUsage(base::BindOnce(
        &DOMStorageContextWrapper::OnStartupUsageRetrieved, wrapper));
  }

  return wrapper;
}

DOMStorageContextWrapper::DOMStorageContextWrapper(
    mojo::Remote<storage::mojom::SessionStorageControl> session_storage_control,
    mojo::Remote<storage::mojom::LocalStorageControl> local_storage_control,
    storage::SpecialStoragePolicy* special_storage_policy)
    : session_storage_control_(std::move(session_storage_control)),
      local_storage_control_(std::move(local_storage_control)),
      storage_policy_(special_storage_policy) {
  memory_pressure_listener_.reset(new base::MemoryPressureListener(
      base::BindRepeating(&DOMStorageContextWrapper::OnMemoryPressure,
                          base::Unretained(this))));
}

DOMStorageContextWrapper::~DOMStorageContextWrapper() {
  DCHECK(!local_storage_control_)
      << "Shutdown should be called before destruction";
}

storage::mojom::SessionStorageControl*
DOMStorageContextWrapper::GetSessionStorageControl() {
  if (!session_storage_control_)
    return nullptr;
  return session_storage_control_.get();
}

storage::mojom::LocalStorageControl*
DOMStorageContextWrapper::GetLocalStorageControl() {
  DCHECK(local_storage_control_);
  return local_storage_control_.get();
}

void DOMStorageContextWrapper::GetLocalStorageUsage(
    GetLocalStorageUsageCallback callback) {
  if (!local_storage_control_) {
    // Shutdown() has been called.
    std::move(callback).Run(std::vector<StorageUsageInfo>());
    return;
  }

  local_storage_control_->GetUsage(
      base::BindOnce(&AdaptLocalStorageUsageInfo, std::move(callback)));
}

void DOMStorageContextWrapper::GetSessionStorageUsage(
    GetSessionStorageUsageCallback callback) {
  if (!session_storage_control_) {
    // Shutdown() has been called.
    std::move(callback).Run(std::vector<SessionStorageUsageInfo>());
    return;
  }

  session_storage_control_->GetUsage(
      base::BindOnce(&AdaptSessionStorageUsageInfo, std::move(callback)));
}

void DOMStorageContextWrapper::DeleteLocalStorage(const url::Origin& origin,
                                                  base::OnceClosure callback) {
  DCHECK(callback);
  if (!local_storage_control_) {
    // Shutdown() has been called.
    std::move(callback).Run();
    return;
  }

  local_storage_control_->DeleteStorage(origin, std::move(callback));
}

void DOMStorageContextWrapper::PerformLocalStorageCleanup(
    base::OnceClosure callback) {
  DCHECK(callback);
  if (!local_storage_control_) {
    // Shutdown() has been called.
    std::move(callback).Run();
    return;
  }

  local_storage_control_->CleanUpStorage(std::move(callback));
}

void DOMStorageContextWrapper::DeleteSessionStorage(
    const SessionStorageUsageInfo& usage_info,
    base::OnceClosure callback) {
  if (!session_storage_control_) {
    // Shutdown() has been called.
    std::move(callback).Run();
    return;
  }
  session_storage_control_->DeleteStorage(
      url::Origin::Create(usage_info.origin), usage_info.namespace_id,
      std::move(callback));
}

void DOMStorageContextWrapper::PerformSessionStorageCleanup(
    base::OnceClosure callback) {
  DCHECK(callback);
  if (!session_storage_control_) {
    // Shutdown() has been called.
    std::move(callback).Run();
    return;
  }

  session_storage_control_->CleanUpStorage(std::move(callback));
}

scoped_refptr<SessionStorageNamespace>
DOMStorageContextWrapper::RecreateSessionStorage(
    const std::string& namespace_id) {
  return SessionStorageNamespaceImpl::Create(this, namespace_id);
}

void DOMStorageContextWrapper::StartScavengingUnusedSessionStorage() {
  if (!session_storage_control_) {
    // Shutdown() has been called.
    return;
  }

  session_storage_control_->ScavengeUnusedNamespaces(base::NullCallback());
}

void DOMStorageContextWrapper::SetForceKeepSessionState() {
  if (!local_storage_control_) {
    // Shutdown() has been called.
    return;
  }

  local_storage_control_->ForceKeepSessionState();
}

void DOMStorageContextWrapper::Shutdown() {
  // Signals the implementation to perform shutdown operations.
  session_storage_control_.reset();
  local_storage_control_.reset();
  memory_pressure_listener_.reset();

  if (storage_policy_observer_) {
    // Make sure the observer drops its reference to |this|.
    storage_policy_observer_.Post(
        FROM_HERE, &StoragePolicyObserver::DidShutdownContextWrapper);
  }
}

void DOMStorageContextWrapper::Flush() {
  if (session_storage_control_)
    session_storage_control_->Flush(base::NullCallback());
  if (local_storage_control_)
    local_storage_control_->Flush(base::NullCallback());
}

void DOMStorageContextWrapper::OpenLocalStorage(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::StorageArea> receiver) {
  DCHECK(local_storage_control_);
  local_storage_control_->BindStorageArea(origin, std::move(receiver));
  if (storage_policy_) {
    EnsureLocalStorageOriginIsTracked(origin);
    OnStoragePolicyChanged();
  }
}

void DOMStorageContextWrapper::BindNamespace(
    const std::string& namespace_id,
    mojo::ReportBadMessageCallback bad_message_callback,
    mojo::PendingReceiver<blink::mojom::SessionStorageNamespace> receiver) {
  DCHECK(session_storage_control_);
  session_storage_control_->BindNamespace(
      namespace_id, std::move(receiver),
      base::BindOnce(&HandleSessionStorageBindingResult, namespace_id,
                     std::move(bad_message_callback)));
}

void DOMStorageContextWrapper::BindStorageArea(
    ChildProcessSecurityPolicyImpl::Handle security_policy_handle,
    const url::Origin& origin,
    const std::string& namespace_id,
    mojo::ReportBadMessageCallback bad_message_callback,
    mojo::PendingReceiver<blink::mojom::StorageArea> receiver) {
  if (!security_policy_handle.CanAccessDataForOrigin(origin)) {
    std::move(bad_message_callback)
        .Run("Access denied for sessionStorage request");
    return;
  }

  DCHECK(session_storage_control_);
  session_storage_control_->BindStorageArea(
      origin, namespace_id, std::move(receiver),
      base::BindOnce(&HandleSessionStorageBindingResult, namespace_id,
                     std::move(bad_message_callback)));
}

scoped_refptr<SessionStorageNamespaceImpl>
DOMStorageContextWrapper::MaybeGetExistingNamespace(
    const std::string& namespace_id) const {
  base::AutoLock lock(alive_namespaces_lock_);
  auto it = alive_namespaces_.find(namespace_id);
  return (it != alive_namespaces_.end()) ? it->second : nullptr;
}

void DOMStorageContextWrapper::AddNamespace(
    const std::string& namespace_id,
    SessionStorageNamespaceImpl* session_namespace) {
  base::AutoLock lock(alive_namespaces_lock_);
  DCHECK(alive_namespaces_.find(namespace_id) == alive_namespaces_.end());
  alive_namespaces_[namespace_id] = session_namespace;
}

void DOMStorageContextWrapper::RemoveNamespace(
    const std::string& namespace_id) {
  base::AutoLock lock(alive_namespaces_lock_);
  DCHECK(alive_namespaces_.find(namespace_id) != alive_namespaces_.end());
  alive_namespaces_.erase(namespace_id);
}

void DOMStorageContextWrapper::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  PurgeOption purge_option = PURGE_UNOPENED;
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    purge_option = PURGE_AGGRESSIVE;
  }
  PurgeMemory(purge_option);
}

void DOMStorageContextWrapper::PurgeMemory(PurgeOption purge_option) {
  if (!local_storage_control_) {
    // Shutdown was called.
    return;
  }

  if (purge_option == PURGE_AGGRESSIVE) {
    DCHECK(session_storage_control_);
    session_storage_control_->PurgeMemory();
    local_storage_control_->PurgeMemory();
  }
}

void DOMStorageContextWrapper::OnStartupUsageRetrieved(
    std::vector<storage::mojom::LocalStorageUsageInfoPtr> usage) {
  for (const auto& info : usage)
    EnsureLocalStorageOriginIsTracked(info->origin);
  OnStoragePolicyChanged();
}

void DOMStorageContextWrapper::EnsureLocalStorageOriginIsTracked(
    const url::Origin& origin) {
  DCHECK(storage_policy_);
  auto it = local_storage_origins_.find(origin);
  if (it == local_storage_origins_.end())
    local_storage_origins_[origin] = {};
}

void DOMStorageContextWrapper::OnStoragePolicyChanged() {
  if (!local_storage_control_)
    return;

  // Scan for any relevant changes to policy regarding origins we know we're
  // managing.
  std::vector<storage::mojom::LocalStoragePolicyUpdatePtr> policy_updates;
  for (auto& entry : local_storage_origins_) {
    const url::Origin& origin = entry.first;
    LocalStorageOriginState& state = entry.second;
    state.should_purge_on_shutdown = ShouldPurgeLocalStorageOnShutdown(origin);
    if (state.should_purge_on_shutdown != state.will_purge_on_shutdown) {
      state.will_purge_on_shutdown = state.should_purge_on_shutdown;
      policy_updates.push_back(storage::mojom::LocalStoragePolicyUpdate::New(
          origin, state.should_purge_on_shutdown));
    }
  }

  if (!policy_updates.empty())
    local_storage_control_->ApplyPolicyUpdates(std::move(policy_updates));
}

bool DOMStorageContextWrapper::ShouldPurgeLocalStorageOnShutdown(
    const url::Origin& origin) {
  if (!storage_policy_)
    return false;
  return storage_policy_->IsStorageSessionOnly(origin.GetURL()) &&
         !storage_policy_->IsStorageProtected(origin.GetURL());
}

}  // namespace content
