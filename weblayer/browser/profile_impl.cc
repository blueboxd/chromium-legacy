// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/profile_impl.h"

#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/resource_context.h"
#include "weblayer/browser/browser_controller_impl.h"
#include "weblayer/public/download_delegate.h"

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "weblayer/browser/java/jni/ProfileImpl_jni.h"
#endif

#if defined(OS_ANDROID)
using base::android::AttachCurrentThread;
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

class DownloadManagerDelegateImpl : public content::DownloadManagerDelegate {
 public:
  DownloadManagerDelegateImpl() = default;
  ~DownloadManagerDelegateImpl() override = default;

  bool InterceptDownloadIfApplicable(
      const GURL& url,
      const std::string& user_agent,
      const std::string& content_disposition,
      const std::string& mime_type,
      const std::string& request_origin,
      int64_t content_length,
      bool is_transient,
      content::WebContents* web_contents) override {
    // If there's no DownloadDelegate, the download is simply dropped.
    auto* browser = BrowserControllerImpl::FromWebContents(web_contents);
    if (!browser)
      return true;

    DownloadDelegate* delegate = browser->download_delegate();
    if (!delegate)
      return true;

    delegate->DownloadRequested(url, user_agent, content_disposition, mime_type,
                                content_length);
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadManagerDelegateImpl);
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
    return &download_delegate_;
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
  DownloadManagerDelegateImpl download_delegate_;

  DISALLOW_COPY_AND_ASSIGN(BrowserContextImpl);
};

class ProfileImpl::DataClearer : public content::BrowsingDataRemover::Observer {
 public:
  DataClearer(content::BrowserContext* browser_context, ProfileImpl* profile)
      : remover_(
            content::BrowserContext::GetBrowsingDataRemover(browser_context)),
        profile_(profile) {
    remover_->AddObserver(this);
  }

  ~DataClearer() override { remover_->RemoveObserver(this); }

  void ClearData() {
    int mask = content::BrowsingDataRemover::DATA_TYPE_COOKIES |
               content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES;
    int origin_types =
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
        content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;
    remover_->RemoveAndReply(base::Time(), base::Time::Max(), mask,
                             origin_types, this);
  }

  void OnBrowsingDataRemoverDone() override {
    profile_->OnBrowsingDataCleared();
  }

 private:
  content::BrowsingDataRemover* const remover_;
  ProfileImpl* const profile_;
};

ProfileImpl::ProfileImpl(const base::FilePath& path) : path_(path) {
  browser_context_ = std::make_unique<BrowserContextImpl>(path_);
}

ProfileImpl::~ProfileImpl() = default;

content::BrowserContext* ProfileImpl::GetBrowserContext() {
  return browser_context_.get();
}

void ProfileImpl::OnBrowsingDataCleared() {
#if defined(OS_ANDROID)
  Java_ProfileImpl_onBrowsingDataCleared(AttachCurrentThread(), java_profile_);
#endif
}

void ProfileImpl::ClearBrowsingData() {
  if (!data_clearer_) {
    data_clearer_ = std::make_unique<DataClearer>(browser_context_.get(), this);
  }
  data_clearer_->ClearData();
}

std::unique_ptr<Profile> Profile::Create(const base::FilePath& path) {
  return std::make_unique<ProfileImpl>(path);
}

#if defined(OS_ANDROID)
ProfileImpl::ProfileImpl(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_profile,
    const base::android::JavaParamRef<jstring>& path)
    : ProfileImpl(base::FilePath(ConvertJavaStringToUTF8(env, path))) {
  java_profile_.Reset(env, java_profile);
}

static jlong JNI_ProfileImpl_CreateProfile(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_profile,
    const base::android::JavaParamRef<jstring>& path) {
  return reinterpret_cast<jlong>(
      new weblayer::ProfileImpl(env, java_profile, path));
}

static void JNI_ProfileImpl_DeleteProfile(JNIEnv* env, jlong profile) {
  delete reinterpret_cast<ProfileImpl*>(profile);
}
#endif  // OS_ANDROID

}  // namespace weblayer
