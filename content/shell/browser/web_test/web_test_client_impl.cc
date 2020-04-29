// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/web_test_client_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/content_index_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/web_test/web_test_browser_context.h"
#include "content/shell/browser/web_test/web_test_content_browser_client.h"
#include "content/shell/browser/web_test/web_test_content_index_provider.h"
#include "content/shell/browser/web_test/web_test_control_host.h"
#include "content/shell/browser/web_test/web_test_permission_manager.h"
#include "content/shell/common/web_test/web_test_constants.h"
#include "content/test/mock_platform_notification_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/browser/quota/quota_manager.h"

namespace content {

namespace {

MockPlatformNotificationService* GetMockPlatformNotificationService() {
  auto* client = WebTestContentBrowserClient::Get();
  auto* context = client->GetWebTestBrowserContext();
  auto* service = client->GetPlatformNotificationService(context);
  return static_cast<MockPlatformNotificationService*>(service);
}

WebTestContentIndexProvider* GetWebTestContentIndexProvider() {
  auto* client = WebTestContentBrowserClient::Get();
  auto* context = client->GetWebTestBrowserContext();
  return static_cast<WebTestContentIndexProvider*>(
      context->GetContentIndexProvider());
}

ContentIndexContext* GetContentIndexContext(const url::Origin& origin) {
  auto* client = WebTestContentBrowserClient::Get();
  auto* context = client->GetWebTestBrowserContext();
  auto* storage_partition = BrowserContext::GetStoragePartitionForSite(
      context, origin.GetURL(), /* can_create= */ false);
  return storage_partition->GetContentIndexContext();
}

void SetDatabaseQuotaOnIOThread(
    scoped_refptr<storage::QuotaManager> quota_manager,
    int32_t quota) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(quota >= 0 || quota == kDefaultDatabaseQuota);
  if (quota == kDefaultDatabaseQuota) {
    // Reset quota to settings with a zero refresh interval to force
    // QuotaManager to refresh settings immediately.
    storage::QuotaSettings default_settings;
    default_settings.refresh_interval = base::TimeDelta();
    quota_manager->SetQuotaSettings(default_settings);
  } else {
    quota_manager->SetQuotaSettings(storage::GetHardCodedSettings(quota));
  }
}

}  // namespace

// static
void WebTestClientImpl::Create(
    int render_process_id,
    storage::QuotaManager* quota_manager,
    storage::DatabaseTracker* database_tracker,
    network::mojom::NetworkContext* network_context,
    mojo::PendingAssociatedReceiver<mojom::WebTestClient> receiver) {
  mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<WebTestClientImpl>(render_process_id, quota_manager,
                                          database_tracker, network_context),
      std::move(receiver));
}

WebTestClientImpl::WebTestClientImpl(
    int render_process_id,
    storage::QuotaManager* quota_manager,
    storage::DatabaseTracker* database_tracker,
    network::mojom::NetworkContext* network_context)
    : render_process_id_(render_process_id),
      quota_manager_(quota_manager),
      database_tracker_(database_tracker),
      network_context_(network_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  network_context->GetCookieManager(
      cookie_manager_.BindNewPipeAndPassReceiver());
}

WebTestClientImpl::~WebTestClientImpl() = default;

void WebTestClientImpl::SimulateWebNotificationClick(
    const std::string& title,
    int32_t action_index,
    const base::Optional<base::string16>& reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetMockPlatformNotificationService()->SimulateClick(
      title,
      action_index == std::numeric_limits<int32_t>::min()
          ? base::Optional<int>()
          : base::Optional<int>(action_index),
      reply);
}

void WebTestClientImpl::SimulateWebNotificationClose(const std::string& title,
                                                     bool by_user) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetMockPlatformNotificationService()->SimulateClose(title, by_user);
}

void WebTestClientImpl::SimulateWebContentIndexDelete(const std::string& id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* provider = GetWebTestContentIndexProvider();

  std::pair<int64_t, url::Origin> registration_data =
      provider->GetRegistrationDataFromId(id);

  auto* context = GetContentIndexContext(registration_data.second);
  context->OnUserDeletedItem(registration_data.first, registration_data.second,
                             id);
}

void WebTestClientImpl::ResetPermissions() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebTestContentBrowserClient::Get()
      ->GetWebTestBrowserContext()
      ->GetWebTestPermissionManager()
      ->ResetPermissions();
}

void WebTestClientImpl::SetPermission(const std::string& name,
                                      blink::mojom::PermissionStatus status,
                                      const GURL& origin,
                                      const GURL& embedding_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::PermissionType type;
  if (name == "midi") {
    type = PermissionType::MIDI;
  } else if (name == "midi-sysex") {
    type = PermissionType::MIDI_SYSEX;
  } else if (name == "push-messaging" || name == "notifications") {
    type = PermissionType::NOTIFICATIONS;
  } else if (name == "geolocation") {
    type = PermissionType::GEOLOCATION;
  } else if (name == "protected-media-identifier") {
    type = PermissionType::PROTECTED_MEDIA_IDENTIFIER;
  } else if (name == "background-sync") {
    type = PermissionType::BACKGROUND_SYNC;
  } else if (name == "accessibility-events") {
    type = PermissionType::ACCESSIBILITY_EVENTS;
  } else if (name == "clipboard-read-write") {
    type = PermissionType::CLIPBOARD_READ_WRITE;
  } else if (name == "clipboard-sanitized-write") {
    type = PermissionType::CLIPBOARD_SANITIZED_WRITE;
  } else if (name == "payment-handler") {
    type = PermissionType::PAYMENT_HANDLER;
  } else if (name == "accelerometer" || name == "gyroscope" ||
             name == "magnetometer" || name == "ambient-light-sensor") {
    type = PermissionType::SENSORS;
  } else if (name == "background-fetch") {
    type = PermissionType::BACKGROUND_FETCH;
  } else if (name == "periodic-background-sync") {
    type = PermissionType::PERIODIC_BACKGROUND_SYNC;
  } else if (name == "wake-lock-screen") {
    type = PermissionType::WAKE_LOCK_SCREEN;
  } else if (name == "wake-lock-system") {
    type = PermissionType::WAKE_LOCK_SYSTEM;
  } else if (name == "nfc") {
    type = PermissionType::NFC;
  } else if (name == "storage-access") {
    type = PermissionType::STORAGE_ACCESS_GRANT;
  } else {
    NOTREACHED();
    type = PermissionType::NOTIFICATIONS;
  }

  WebTestContentBrowserClient::Get()
      ->GetWebTestBrowserContext()
      ->GetWebTestPermissionManager()
      ->SetPermission(type, status, origin, embedding_origin);
}

void WebTestClientImpl::WebTestRuntimeFlagsChanged(
    base::Value changed_web_test_runtime_flags) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!WebTestControlHost::Get())
    return;

  base::DictionaryValue* changed_web_test_runtime_flags_dictionary = nullptr;
  bool ok = changed_web_test_runtime_flags.GetAsDictionary(
      &changed_web_test_runtime_flags_dictionary);
  DCHECK(ok);

  WebTestControlHost::Get()->OnWebTestRuntimeFlagsChanged(
      render_process_id_, *changed_web_test_runtime_flags_dictionary);
}

void WebTestClientImpl::DeleteAllCookies() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  cookie_manager_->DeleteCookies(network::mojom::CookieDeletionFilter::New(),
                                 base::BindOnce([](uint32_t) {}));
}

void WebTestClientImpl::RegisterIsolatedFileSystem(
    const std::vector<base::FilePath>& absolute_filenames,
    RegisterIsolatedFileSystemCallback callback) {
  storage::IsolatedContext::FileInfoSet files;
  std::string filesystem_id;
  ChildProcessSecurityPolicy* policy =
      ChildProcessSecurityPolicy::GetInstance();
  for (auto& filename : absolute_filenames) {
    files.AddPath(filename, nullptr);
    if (!policy->CanReadFile(render_process_id_, filename))
      policy->GrantReadFile(render_process_id_, filename);
  }
  filesystem_id =
      storage::IsolatedContext::GetInstance()->RegisterDraggedFileSystem(files);
  policy->GrantReadFileSystem(render_process_id_, filesystem_id);
  std::move(callback).Run(filesystem_id);
}

void WebTestClientImpl::ClearAllDatabases() {
  database_tracker_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<storage::DatabaseTracker> db_tracker) {
            DCHECK(db_tracker->task_runner()->RunsTasksInCurrentSequence());
            db_tracker->DeleteDataModifiedSince(base::Time(),
                                                net::CompletionOnceCallback());
          },
          database_tracker_));
}

void WebTestClientImpl::SetDatabaseQuota(int32_t quota) {
  base::PostTask(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&SetDatabaseQuotaOnIOThread, quota_manager_, quota));
}

void WebTestClientImpl::SetTrustTokenKeyCommitments(
    const std::string& raw_commitments,
    base::OnceClosure callback) {
  GetNetworkService()->SetTrustTokenKeyCommitments(raw_commitments,
                                                   std::move(callback));
}

void WebTestClientImpl::ClearTrustTokenState(base::OnceClosure callback) {
  // nullptr denotes a wildcard filter.
  network_context_->ClearTrustTokenData(nullptr, std::move(callback));
}

}  // namespace content
