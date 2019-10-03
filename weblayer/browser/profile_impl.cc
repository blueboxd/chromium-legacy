// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/profile_impl.h"

#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/resource_context.h"

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "weblayer/browser/java/jni/ProfileImpl_jni.h"
#endif

namespace weblayer {

namespace {

class ResourceContextImpl : public content::ResourceContext {
 public:
  ResourceContextImpl() = default;
  ~ResourceContextImpl() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceContextImpl);
};

}  // namespace

class ProfileImpl::BrowserContextImpl : public content::BrowserContext {
 public:
  BrowserContextImpl(const base::FilePath& path) : path_(path) {
    resource_context_ = std::make_unique<ResourceContextImpl>();
    content::BrowserContext::Initialize(this, path_);
  }

  ~BrowserContextImpl() override { NotifyWillBeDestroyed(this); }

  // BrowserContext implementation:
#if !defined(OS_ANDROID)
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath&) override {
    return nullptr;
  }
#endif  // !defined(OS_ANDROID)

  base::FilePath GetPath() override { return path_; }

  bool IsOffTheRecord() override { return path_.empty(); }

  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override {
    return nullptr;
  }

  content::ResourceContext* GetResourceContext() override {
    return resource_context_.get();
  }

  content::BrowserPluginGuestManager* GetGuestManager() override {
    return nullptr;
  }

  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override {
    return nullptr;
  }

  content::PushMessagingService* GetPushMessagingService() override {
    return nullptr;
  }

  content::StorageNotificationService* GetStorageNotificationService()
      override {
    return nullptr;
  }

  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override {
    return nullptr;
  }

  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override {
    return nullptr;
  }

  content::ClientHintsControllerDelegate* GetClientHintsControllerDelegate()
      override {
    return nullptr;
  }

  content::BackgroundFetchDelegate* GetBackgroundFetchDelegate() override {
    return nullptr;
  }

  content::BackgroundSyncController* GetBackgroundSyncController() override {
    return nullptr;
  }

  content::BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate()
      override {
    return nullptr;
  }

  content::ContentIndexProvider* GetContentIndexProvider() override {
    return nullptr;
  }

 private:
  base::FilePath path_;
  std::unique_ptr<ResourceContextImpl> resource_context_;

  DISALLOW_COPY_AND_ASSIGN(BrowserContextImpl);
};

ProfileImpl::ProfileImpl(const base::FilePath& path) : path_(path) {
  browser_context_ = std::make_unique<BrowserContextImpl>(path_);
}

ProfileImpl::~ProfileImpl() = default;

content::BrowserContext* ProfileImpl::GetBrowserContext() {
  return browser_context_.get();
}

void ProfileImpl::ClearBrowsingData() {
  NOTIMPLEMENTED();
}

std::unique_ptr<Profile> Profile::Create(const base::FilePath& path) {
  return std::make_unique<ProfileImpl>(path);
}

#if defined(OS_ANDROID)
static jlong JNI_ProfileImpl_CreateProfile(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& path) {
  return reinterpret_cast<jlong>(new weblayer::ProfileImpl(
      base::FilePath(ConvertJavaStringToUTF8(env, path))));
}

static void JNI_ProfileImpl_DeleteProfile(JNIEnv* env, jlong profile) {
  delete reinterpret_cast<ProfileImpl*>(profile);
}
#endif  // OS_ANDROID

}  // namespace weblayer
