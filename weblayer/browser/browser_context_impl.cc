// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_context_impl.h"

#include "components/download/public/common/in_progress_download_manager.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/resource_context.h"
#include "weblayer/browser/fake_permission_controller_delegate.h"
#include "weblayer/public/common/switches.h"

#if defined(OS_ANDROID)
#include "base/android/path_utils.h"
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
}

BrowserContextImpl::~BrowserContextImpl() {
  NotifyWillBeDestroyed(this);
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
  return &ssl_host_state_delegate_;
}

content::PermissionControllerDelegate*
BrowserContextImpl::GetPermissionControllerDelegate() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebLayerFakePermissions)) {
    if (!permission_controller_delegate_) {
      permission_controller_delegate_ =
          std::make_unique<FakePermissionControllerDelegate>();
    }
    return permission_controller_delegate_.get();
  }
  return nullptr;
}

content::ClientHintsControllerDelegate*
BrowserContextImpl::GetClientHintsControllerDelegate() {
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
      nullptr, base::FilePath(), nullptr,
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
  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
  RegisterPrefs(pref_registry.get());

  PrefServiceFactory pref_service_factory;
  pref_service_factory.set_user_prefs(
      base::MakeRefCounted<InMemoryPrefStore>());
  user_pref_service_ = pref_service_factory.Create(pref_registry);
  // Note: UserPrefs::Set also ensures that the user_pref_service_ has not
  // been set previously.
  user_prefs::UserPrefs::Set(this, user_pref_service_.get());
}

void BrowserContextImpl::RegisterPrefs(PrefRegistrySimple* pref_registry) {
  safe_browsing::RegisterProfilePrefs(pref_registry);
}

}  // namespace weblayer
