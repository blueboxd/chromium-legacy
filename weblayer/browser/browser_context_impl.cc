// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_context_impl.h"

#include "base/threading/thread_restrictions.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/download/public/common/in_progress_download_manager.h"
#include "components/embedder_support/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/permissions/permission_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/variations_client.h"
#include "components/variations/variations_http_header_provider.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/storage_partition.h"
#include "weblayer/browser/permissions/permission_manager_factory.h"
#include "weblayer/browser/stateful_ssl_host_state_delegate_factory.h"
#include "weblayer/public/common/switches.h"

#if defined(OS_ANDROID)
#include "base/android/path_utils.h"
#include "components/cdm/browser/media_drm_storage_impl.h"  // nogncheck
#include "components/permissions/contexts/geolocation_permission_context_android.h"
#elif defined(OS_WIN)
#include <KnownFolders.h>
#include <shlobj.h>
#include "base/win/scoped_co_mem.h"
#elif defined(OS_POSIX)
#include "base/nix/xdg_util.h"
#endif

namespace weblayer {

namespace {

// Ignores origin security check. DownloadManagerImpl will provide its own
// implementation when InProgressDownloadManager object is passed to it.
bool IgnoreOriginSecurityCheck(const GURL& url) {
  return true;
}

void BindWakeLockProvider(
    mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) {
  content::GetDeviceService().BindWakeLockProvider(std::move(receiver));
}

}  // namespace

class ResourceContextImpl : public content::ResourceContext {
 public:
  ResourceContextImpl() = default;
  ~ResourceContextImpl() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceContextImpl);
};

BrowserContextImpl::BrowserContextImpl(ProfileImpl* profile_impl,
                                       const base::FilePath& path)
    : profile_impl_(profile_impl),
      path_(path),
      resource_context_(new ResourceContextImpl()),
      download_delegate_(BrowserContext::GetDownloadManager(this)) {
  content::BrowserContext::Initialize(this, path_);

  CreateUserPrefService();

  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      this);
}

BrowserContextImpl::~BrowserContextImpl() {
  NotifyWillBeDestroyed(this);

  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      this);
}

base::FilePath BrowserContextImpl::GetDefaultDownloadDirectory() {
  // Note: if we wanted to productionize this on Windows/Linux, refactor
  // src/chrome's GetDefaultDownloadDirectory.
  base::FilePath download_dir;
#if defined(OS_ANDROID)
  base::android::GetDownloadsDirectory(&download_dir);
#elif defined(OS_WIN)
  base::win::ScopedCoMem<wchar_t> path_buf;
  if (SUCCEEDED(
          SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &path_buf))) {
    download_dir = base::FilePath(path_buf.get());
  } else {
    NOTREACHED();
  }
#else
  download_dir = base::nix::GetXDGUserDirectory("DOWNLOAD", "Downloads");
#endif
  return download_dir;
}

#if !defined(OS_ANDROID)
std::unique_ptr<content::ZoomLevelDelegate>
BrowserContextImpl::CreateZoomLevelDelegate(const base::FilePath&) {
  return nullptr;
}
#endif  // !defined(OS_ANDROID)

base::FilePath BrowserContextImpl::GetPath() {
  return path_;
}

bool BrowserContextImpl::IsOffTheRecord() {
  return path_.empty();
}

content::DownloadManagerDelegate*
BrowserContextImpl::GetDownloadManagerDelegate() {
  return &download_delegate_;
}

content::ResourceContext* BrowserContextImpl::GetResourceContext() {
  return resource_context_.get();
}

content::BrowserPluginGuestManager* BrowserContextImpl::GetGuestManager() {
  return nullptr;
}

storage::SpecialStoragePolicy* BrowserContextImpl::GetSpecialStoragePolicy() {
  return nullptr;
}

content::PushMessagingService* BrowserContextImpl::GetPushMessagingService() {
  return nullptr;
}

content::StorageNotificationService*
BrowserContextImpl::GetStorageNotificationService() {
  return nullptr;
}

content::SSLHostStateDelegate* BrowserContextImpl::GetSSLHostStateDelegate() {
  return StatefulSSLHostStateDelegateFactory::GetForBrowserContext(this);
}

content::PermissionControllerDelegate*
BrowserContextImpl::GetPermissionControllerDelegate() {
  return PermissionManagerFactory::GetForBrowserContext(this);
}

content::ClientHintsControllerDelegate*
BrowserContextImpl::GetClientHintsControllerDelegate() {
  // TODO(crbug.com/1065537): implement me.
  return nullptr;
}

content::BackgroundFetchDelegate*
BrowserContextImpl::GetBackgroundFetchDelegate() {
  return nullptr;
}

content::BackgroundSyncController*
BrowserContextImpl::GetBackgroundSyncController() {
  return nullptr;
}

content::BrowsingDataRemoverDelegate*
BrowserContextImpl::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

download::InProgressDownloadManager*
BrowserContextImpl::RetriveInProgressDownloadManager() {
  // Override this to provide a connection to the wake lock service.
  auto* download_manager = new download::InProgressDownloadManager(
      nullptr, path_,
      path_.empty()
          ? nullptr
          : GetDefaultStoragePartition(this)->GetProtoDatabaseProvider(),
      base::BindRepeating(&IgnoreOriginSecurityCheck),
      base::BindRepeating(&content::DownloadRequestUtils::IsURLSafe),
      base::BindRepeating(&BindWakeLockProvider));

#if defined(OS_ANDROID)
  download_manager->set_default_download_dir(GetDefaultDownloadDirectory());
#endif

  return download_manager;
}

content::ContentIndexProvider* BrowserContextImpl::GetContentIndexProvider() {
  return nullptr;
}

void BrowserContextImpl::CreateUserPrefService() {
  auto pref_registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();
  RegisterPrefs(pref_registry.get());

  PrefServiceFactory pref_service_factory;
  if (IsOffTheRecord()) {
    pref_service_factory.set_user_prefs(
        base::MakeRefCounted<InMemoryPrefStore>());
  } else {
    pref_service_factory.set_user_prefs(base::MakeRefCounted<JsonPrefStore>(
        path_.Append(FILE_PATH_LITERAL("Preferences"))));
  }
  {
    // Creating the prefs service may require reading the preferences from disk.
    base::ScopedAllowBlocking allow_io;
    user_pref_service_ = pref_service_factory.Create(pref_registry);
  }
  // Note: UserPrefs::Set also ensures that the user_pref_service_ has not
  // been set previously.
  user_prefs::UserPrefs::Set(this, user_pref_service_.get());
}

void BrowserContextImpl::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* pref_registry) {
  // This pref is used by captive_portal::CaptivePortalService (as well as other
  // potential use cases in the future, as it is used for various purposes
  // through //chrome).
  pref_registry->RegisterBooleanPref(
      embedder_support::kAlternateErrorPagesEnabled, true);

  StatefulSSLHostStateDelegate::RegisterProfilePrefs(pref_registry);
  HostContentSettingsMap::RegisterProfilePrefs(pref_registry);
  safe_browsing::RegisterProfilePrefs(pref_registry);
#if defined(OS_ANDROID)
  cdm::MediaDrmStorageImpl::RegisterProfilePrefs(pref_registry);
  permissions::GeolocationPermissionContextAndroid::RegisterProfilePrefs(
      pref_registry);
#endif
}

class BrowserContextImpl::WebLayerVariationsClient
    : public variations::VariationsClient {
 public:
  explicit WebLayerVariationsClient(content::BrowserContext* browser_context)
      : browser_context_(browser_context) {}

  ~WebLayerVariationsClient() override = default;

  bool IsIncognito() const override {
    return browser_context_->IsOffTheRecord();
  }

  std::string GetVariationsHeader() const override {
    return variations::VariationsHttpHeaderProvider::GetInstance()
        ->GetClientDataHeader(IsSignedIn());
  }

 private:
  bool IsSignedIn() const {
    // TODO(weblayer-dev): Update when signin is supported.
    return false;
  }

  content::BrowserContext* browser_context_;
};

variations::VariationsClient* BrowserContextImpl::GetVariationsClient() {
  if (!weblayer_variations_client_) {
    weblayer_variations_client_ =
        std::make_unique<WebLayerVariationsClient>(this);
  }
  return weblayer_variations_client_.get();
}

}  // namespace weblayer
