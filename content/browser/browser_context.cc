// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_context.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/supports_user_data.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/browser/background_sync/background_sync_scheduler.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/browsing_data/browsing_data_remover_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/content_service_delegate_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/media/browser_feature_provider.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/push_messaging/push_messaging_router.h"
#include "content/browser/storage_partition_impl_map.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "media/base/media_switches.h"
#include "media/capabilities/in_memory_video_decode_stats_db_impl.h"
#include "media/capabilities/video_decode_stats_db_impl.h"
#include "media/learning/common/media_learning_tasks.h"
#include "media/learning/impl/learning_session_impl.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "services/content/service.h"
#include "services/network/public/cpp/features.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/file_system/external_mount_points.h"

using base::UserDataAdapter;

namespace content {

namespace {

class ContentServiceHolder : public base::SupportsUserData::Data {
 public:
  explicit ContentServiceHolder(BrowserContext* browser_context)
      : delegate_(browser_context) {
    delegate_.AddService(&service_);
  }

  ~ContentServiceHolder() override = default;

  content::Service& service() { return service_; }

 private:
  ContentServiceDelegateImpl delegate_;
  content::Service service_{&delegate_};

  DISALLOW_COPY_AND_ASSIGN(ContentServiceHolder);
};

// Key names on BrowserContext.
const char kBrowsingDataRemoverKey[] = "browsing-data-remover";
const char kContentServiceKey[] = "content-service";
const char kDownloadManagerKeyName[] = "download_manager";
const char kPermissionControllerKey[] = "permission-controller";
const char kStoragePartitionMapKeyName[] = "content_storage_partition_map";
const char kVideoDecodePerfHistoryId[] = "video-decode-perf-history";
const char kLearningSession[] = "learning-session";

#if defined(OS_CHROMEOS)
const char kMountPointsKey[] = "mount_points";
#endif  // defined(OS_CHROMEOS)

StoragePartitionImplMap* GetStoragePartitionMap(
    BrowserContext* browser_context) {
  StoragePartitionImplMap* partition_map =
      static_cast<StoragePartitionImplMap*>(
          browser_context->GetUserData(kStoragePartitionMapKeyName));
  if (!partition_map) {
    auto partition_map_owned =
        std::make_unique<StoragePartitionImplMap>(browser_context);
    partition_map = partition_map_owned.get();
    browser_context->SetUserData(kStoragePartitionMapKeyName,
                                 std::move(partition_map_owned));
  }
  return partition_map;
}

StoragePartition* GetStoragePartitionFromConfig(
    BrowserContext* browser_context,
    const std::string& partition_domain,
    const std::string& partition_name,
    bool in_memory,
    bool can_create) {
  StoragePartitionImplMap* partition_map =
      GetStoragePartitionMap(browser_context);

  if (browser_context->IsOffTheRecord())
    in_memory = true;

  return partition_map->Get(partition_domain, partition_name, in_memory,
                            can_create);
}

void SaveSessionStateOnIOThread(AppCacheServiceImpl* appcache_service) {
  appcache_service->set_force_keep_session_state();
}

void ShutdownServiceWorkerContext(StoragePartition* partition) {
  ServiceWorkerContextWrapper* wrapper =
      static_cast<ServiceWorkerContextWrapper*>(
          partition->GetServiceWorkerContext());
  wrapper->process_manager()->Shutdown();
}

void SetDownloadManager(
    BrowserContext* context,
    std::unique_ptr<content::DownloadManager> download_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(download_manager);
  context->SetUserData(kDownloadManagerKeyName, std::move(download_manager));
}

base::WeakPtr<storage::BlobStorageContext> BlobStorageContextGetterForBrowser(
    scoped_refptr<ChromeBlobStorageContext> blob_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return blob_context->context()->AsWeakPtr();
}

}  // namespace

// static
void BrowserContext::AsyncObliterateStoragePartition(
    BrowserContext* browser_context,
    const std::string& partition_domain,
    base::OnceClosure on_gc_required) {
  GetStoragePartitionMap(browser_context)
      ->AsyncObliterate(partition_domain, std::move(on_gc_required));
}

// static
void BrowserContext::GarbageCollectStoragePartitions(
    BrowserContext* browser_context,
    std::unique_ptr<std::unordered_set<base::FilePath>> active_paths,
    base::OnceClosure done) {
  GetStoragePartitionMap(browser_context)
      ->GarbageCollect(std::move(active_paths), std::move(done));
}

DownloadManager* BrowserContext::GetDownloadManager(BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context->GetUserData(kDownloadManagerKeyName)) {
    DownloadManager* download_manager = new DownloadManagerImpl(context);

    SetDownloadManager(context, base::WrapUnique(download_manager));
    download_manager->SetDelegate(context->GetDownloadManagerDelegate());
  }

  return static_cast<DownloadManager*>(
      context->GetUserData(kDownloadManagerKeyName));
}

// static
storage::ExternalMountPoints* BrowserContext::GetMountPoints(
    BrowserContext* context) {
  // Ensure that these methods are called on the UI thread, except for
  // unittests where a UI thread might not have been created.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));

#if defined(OS_CHROMEOS)
  if (!context->GetUserData(kMountPointsKey)) {
    scoped_refptr<storage::ExternalMountPoints> mount_points =
        storage::ExternalMountPoints::CreateRefCounted();
    context->SetUserData(
        kMountPointsKey,
        std::make_unique<UserDataAdapter<storage::ExternalMountPoints>>(
            mount_points.get()));
  }

  return UserDataAdapter<storage::ExternalMountPoints>::Get(context,
                                                            kMountPointsKey);
#else
  return nullptr;
#endif
}

// static
content::BrowsingDataRemover* content::BrowserContext::GetBrowsingDataRemover(
    BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context->GetUserData(kBrowsingDataRemoverKey)) {
    std::unique_ptr<BrowsingDataRemoverImpl> remover =
        std::make_unique<BrowsingDataRemoverImpl>(context);
    remover->SetEmbedderDelegate(context->GetBrowsingDataRemoverDelegate());
    context->SetUserData(kBrowsingDataRemoverKey, std::move(remover));
  }

  return static_cast<BrowsingDataRemoverImpl*>(
      context->GetUserData(kBrowsingDataRemoverKey));
}

// static
content::PermissionController* content::BrowserContext::GetPermissionController(
    BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!context->GetUserData(kPermissionControllerKey)) {
    context->SetUserData(kPermissionControllerKey,
                         std::make_unique<PermissionControllerImpl>(context));
  }

  return static_cast<PermissionControllerImpl*>(
      context->GetUserData(kPermissionControllerKey));
}

StoragePartition* BrowserContext::GetStoragePartition(
    BrowserContext* browser_context,
    SiteInstance* site_instance,
    bool can_create) {
  std::string partition_domain;
  std::string partition_name;
  bool in_memory = false;

  if (site_instance) {
    GetContentClient()->browser()->GetStoragePartitionConfigForSite(
        browser_context, site_instance->GetSiteURL(), true, &partition_domain,
        &partition_name, &in_memory);
  }

  return GetStoragePartitionFromConfig(browser_context, partition_domain,
                                       partition_name, in_memory, can_create);
}

StoragePartition* BrowserContext::GetStoragePartitionForSite(
    BrowserContext* browser_context,
    const GURL& site,
    bool can_create) {
  std::string partition_domain;
  std::string partition_name;
  bool in_memory;

  GetContentClient()->browser()->GetStoragePartitionConfigForSite(
      browser_context, site, true, &partition_domain, &partition_name,
      &in_memory);

  return GetStoragePartitionFromConfig(browser_context, partition_domain,
                                       partition_name, in_memory, can_create);
}

void BrowserContext::ForEachStoragePartition(
    BrowserContext* browser_context,
    StoragePartitionCallback callback) {
  StoragePartitionImplMap* partition_map =
      static_cast<StoragePartitionImplMap*>(
          browser_context->GetUserData(kStoragePartitionMapKeyName));
  if (!partition_map)
    return;

  partition_map->ForEach(std::move(callback));
}

size_t BrowserContext::GetStoragePartitionCount(
    BrowserContext* browser_context) {
  StoragePartitionImplMap* partition_map =
      static_cast<StoragePartitionImplMap*>(
          browser_context->GetUserData(kStoragePartitionMapKeyName));
  return partition_map ? partition_map->size() : 0;
}

StoragePartition* BrowserContext::GetDefaultStoragePartition(
    BrowserContext* browser_context) {
  return GetStoragePartition(browser_context, nullptr);
}

// static
void BrowserContext::CreateMemoryBackedBlob(BrowserContext* browser_context,
                                            base::span<const uint8_t> data,
                                            const std::string& content_type,
                                            BlobCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ChromeBlobStorageContext* blob_context =
      ChromeBlobStorageContext::GetFor(browser_context);
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&ChromeBlobStorageContext::CreateMemoryBackedBlob,
                     base::WrapRefCounted(blob_context), data, content_type),
      std::move(callback));
}

// static
BrowserContext::BlobContextGetter BrowserContext::GetBlobStorageContext(
    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context =
      ChromeBlobStorageContext::GetFor(browser_context);
  return base::BindRepeating(&BlobStorageContextGetterForBrowser,
                             chrome_blob_context);
}

// static
mojo::PendingRemote<blink::mojom::Blob> BrowserContext::GetBlobRemote(
    BrowserContext* browser_context,
    const std::string& uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ChromeBlobStorageContext::GetBlobRemote(browser_context, uuid);
}

// static
void BrowserContext::DeliverPushMessage(
    BrowserContext* browser_context,
    const GURL& origin,
    int64_t service_worker_registration_id,
    const std::string& message_id,
    base::Optional<std::string> payload,
    base::OnceCallback<void(blink::mojom::PushDeliveryStatus)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PushMessagingRouter::DeliverMessage(
      browser_context, origin, service_worker_registration_id, message_id,
      std::move(payload), std::move(callback));
}

// static
void BrowserContext::NotifyWillBeDestroyed(BrowserContext* browser_context) {
  TRACE_EVENT1("shutdown", "BrowserContext::NotifyWillBeDestroyed",
               "browser_context", browser_context);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "shutdown", "BrowserContext::NotifyWillBeDestroyed() called.",
      browser_context, "browser_context", browser_context);
  // Make sure NotifyWillBeDestroyed is idempotent.  This helps facilitate the
  // pattern where NotifyWillBeDestroyed is called from *both*
  // ShellBrowserContext and its derived classes (e.g. WebTestBrowserContext).
  if (browser_context->was_notify_will_be_destroyed_called_)
    return;
  browser_context->was_notify_will_be_destroyed_called_ = true;

  // Subclasses of BrowserContext may expect there to be no more
  // RenderProcessHosts using them by the time this function returns. We
  // therefore explicitly tear down embedded Content Service instances now to
  // ensure that all their WebContents (and therefore RPHs) are torn down too.
  browser_context->RemoveUserData(kContentServiceKey);

  // Service Workers must shutdown before the browser context is destroyed,
  // since they keep render process hosts alive and the codebase assumes that
  // render process hosts die before their profile (browser context) dies.
  ForEachStoragePartition(browser_context,
                          base::BindRepeating(ShutdownServiceWorkerContext));

  // Shared workers also keep render process hosts alive, and are expected to
  // return ref counts to 0 after documents close. However, to ensure that
  // hosts are destructed now, forcibly release their ref counts here.
  for (RenderProcessHost::iterator host_iterator =
           RenderProcessHost::AllHostsIterator();
       !host_iterator.IsAtEnd(); host_iterator.Advance()) {
    RenderProcessHost* host = host_iterator.GetCurrentValue();
    if (host->GetBrowserContext() == browser_context) {
      // This will also clean up spare RPH references.
      host->DisableKeepAliveRefCount();
    }
  }
}

void BrowserContext::EnsureResourceContextInitialized(BrowserContext* context) {
  // This will be enough to tickle initialization of BrowserContext if
  // necessary, which initializes ResourceContext. The reason we don't call
  // ResourceContext::InitializeResourceContext() directly here is that
  // ResourceContext initialization may call back into BrowserContext
  // and when that call returns it'll end rewriting its UserData map. It will
  // end up rewriting the same value but this still causes a race condition.
  //
  // See http://crbug.com/115678.
  GetDefaultStoragePartition(context);
}

void BrowserContext::SaveSessionState(BrowserContext* browser_context) {
  StoragePartition* storage_partition =
      BrowserContext::GetDefaultStoragePartition(browser_context);

  storage::DatabaseTracker* database_tracker =
      storage_partition->GetDatabaseTracker();
  database_tracker->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&storage::DatabaseTracker::SetForceKeepSessionState,
                     base::WrapRefCounted(database_tracker)));

  if (BrowserThread::IsThreadInitialized(BrowserThread::IO)) {
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&SaveSessionStateOnIOThread,
                       static_cast<AppCacheServiceImpl*>(
                           storage_partition->GetAppCacheService())));
  }

  storage_partition->GetCookieManagerForBrowserProcess()
      ->SetForceKeepSessionState();

  DOMStorageContextWrapper* dom_storage_context_proxy =
      static_cast<DOMStorageContextWrapper*>(
          storage_partition->GetDOMStorageContext());
  dom_storage_context_proxy->SetForceKeepSessionState();

  auto& indexed_db_control = storage_partition->GetIndexedDBControl();
  indexed_db_control.SetForceKeepSessionState();
}

void BrowserContext::SetDownloadManagerForTesting(
    BrowserContext* browser_context,
    std::unique_ptr<content::DownloadManager> download_manager) {
  SetDownloadManager(browser_context, std::move(download_manager));
}

// static
void BrowserContext::SetPermissionControllerForTesting(
    BrowserContext* browser_context,
    std::unique_ptr<PermissionController> permission_controller) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(permission_controller);
  browser_context->SetUserData(kPermissionControllerKey,
                               std::move(permission_controller));
}

BrowserContext::BrowserContext()
    : unique_id_(base::UnguessableToken::Create().ToString()) {
  TRACE_EVENT1("shutdown", "BrowserContext::BrowserContext", "browser_context",
               this);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("shutdown", "Browser.BrowserContext", this,
                                    "browser_context", this);
}

BrowserContext::~BrowserContext() {
  TRACE_EVENT1("shutdown", "BrowserContext::~BrowserContext", "browser_context",
               this);
  DCHECK(!GetUserData(kStoragePartitionMapKeyName))
      << "StoragePartitionMap is not shut down properly";

  if (!was_notify_will_be_destroyed_called_) {
    NOTREACHED();
    base::debug::DumpWithoutCrashing();
  }

  // Clean up any isolated origins and other security state associated with this
  // BrowserContext.
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  policy->RemoveStateForBrowserContext(*this);

  if (GetUserData(kDownloadManagerKeyName))
    GetDownloadManager(this)->Shutdown();

  TRACE_EVENT_NESTABLE_ASYNC_END1(
      "shutdown", "BrowserContext::NotifyWillBeDestroyed() called.", this,
      "browser_context", this);
  TRACE_EVENT_NESTABLE_ASYNC_END1("shutdown", "Browser.BrowserContext", this,
                                  "browser_context", this);
}

void BrowserContext::ShutdownStoragePartitions() {
  // The BackgroundSyncScheduler keeps raw pointers to partitions; clear it
  // first.
  if (GetUserData(kBackgroundSyncSchedulerKey))
    RemoveUserData(kBackgroundSyncSchedulerKey);

  if (GetUserData(kStoragePartitionMapKeyName))
    RemoveUserData(kStoragePartitionMapKeyName);
}

std::string BrowserContext::GetMediaDeviceIDSalt() {
  return unique_id_;
}

// static
std::string BrowserContext::CreateRandomMediaDeviceIDSalt() {
  return base::UnguessableToken::Create().ToString();
}

const std::string& BrowserContext::UniqueId() {
  return unique_id_;
}

void BrowserContext::BindNavigableContentsFactory(
    mojo::PendingReceiver<content::mojom::NavigableContentsFactory> receiver) {
  auto* service_holder =
      static_cast<ContentServiceHolder*>(GetUserData(kContentServiceKey));
  if (!service_holder) {
    auto new_holder = std::make_unique<ContentServiceHolder>(this);
    service_holder = new_holder.get();
    SetUserData(kContentServiceKey, std::move(new_holder));
  }

  service_holder->service().BindNavigableContentsFactory(std::move(receiver));
}

media::VideoDecodePerfHistory* BrowserContext::GetVideoDecodePerfHistory() {
  media::VideoDecodePerfHistory* decode_history =
      static_cast<media::VideoDecodePerfHistory*>(
          GetUserData(kVideoDecodePerfHistoryId));

  // Lazily created. Note, this does not trigger loading the DB from disk. That
  // occurs later upon first VideoDecodePerfHistory API request that requires DB
  // access. DB operations will not block the UI thread.
  if (!decode_history) {
    const char kUseInMemoryDBParamName[] = "db_in_memory";
    const bool kUseInMemoryDBDefault = false;
    bool use_in_memory_db = base::GetFieldTrialParamByFeatureAsBool(
        media::kMediaCapabilitiesWithParameters, kUseInMemoryDBParamName,
        kUseInMemoryDBDefault);

    std::unique_ptr<media::VideoDecodeStatsDB> stats_db;
    if (use_in_memory_db) {
      stats_db =
          std::make_unique<media::InMemoryVideoDecodeStatsDBImpl>(nullptr);
    } else {
      auto* db_provider =
          GetDefaultStoragePartition(this)->GetProtoDatabaseProvider();

      stats_db = media::VideoDecodeStatsDBImpl::Create(
          GetPath().Append(FILE_PATH_LITERAL("VideoDecodeStats")), db_provider);
    }

    auto new_decode_history = std::make_unique<media::VideoDecodePerfHistory>(
        std::move(stats_db), BrowserFeatureProvider::GetFactoryCB());
    decode_history = new_decode_history.get();

    SetUserData(kVideoDecodePerfHistoryId, std::move(new_decode_history));
  }

  return decode_history;
}

media::learning::LearningSession* BrowserContext::GetLearningSession() {
  media::learning::LearningSession* learning_session =
      static_cast<media::learning::LearningSession*>(
          GetUserData(kLearningSession));

  if (!learning_session) {
    auto new_learning_session =
        std::make_unique<media::learning::LearningSessionImpl>(
            base::SequencedTaskRunnerHandle::Get());

    // Register all the LearningTasks.
    auto cb = base::BindRepeating(
        [](media::learning::LearningSessionImpl* session,
           const media::learning::LearningTask& task) {
          session->RegisterTask(task);
        },
        new_learning_session.get());
    media::learning::MediaLearningTasks::Register(std::move(cb));

    learning_session = new_learning_session.get();

    SetUserData(kLearningSession, std::move(new_learning_session));
  }

  return learning_session;
}

download::InProgressDownloadManager*
BrowserContext::RetriveInProgressDownloadManager() {
  return nullptr;
}

void BrowserContext::SetCorsOriginAccessListForOrigin(
    const url::Origin& source_origin,
    std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
    std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
    base::OnceClosure closure) {
  NOTREACHED() << "Sub-classes should implement this method to communicate "
                  "with NetworkService to bypass CORS checks.";
}

SharedCorsOriginAccessList* BrowserContext::GetSharedCorsOriginAccessList() {
  // Need to return a valid instance regardless of CORS bypass supports.
  static const base::NoDestructor<scoped_refptr<SharedCorsOriginAccessList>>
      empty_list(SharedCorsOriginAccessList::Create());
  return empty_list->get();
}

bool BrowserContext::ShouldEnableOutOfBlinkCors() {
  return base::FeatureList::IsEnabled(network::features::kOutOfBlinkCors);
}

NativeFileSystemPermissionContext*
BrowserContext::GetNativeFileSystemPermissionContext() {
  return nullptr;
}

ContentIndexProvider* BrowserContext::GetContentIndexProvider() {
  return nullptr;
}

bool BrowserContext::CanUseDiskWhenOffTheRecord() {
  return false;
}

variations::VariationsClient* BrowserContext::GetVariationsClient() {
  return nullptr;
}

}  // namespace content
