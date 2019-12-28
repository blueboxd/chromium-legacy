// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_context_impl.h"

#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/resource_context.h"

namespace weblayer {

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
      resource_context_(new ResourceContextImpl()) {
  content::BrowserContext::Initialize(this, path_);

  CreateUserPrefService();
}

BrowserContextImpl::~BrowserContextImpl() {
  NotifyWillBeDestroyed(this);
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
