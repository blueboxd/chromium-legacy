// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/profile_impl.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/common/weblayer_paths.h"

#if defined(OS_ANDROID)
#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "weblayer/browser/java/jni/ProfileImpl_jni.h"
#endif

#if defined(OS_POSIX)
#include "base/base_paths_posix.h"
#endif

#if defined(OS_ANDROID)
using base::android::AttachCurrentThread;
#endif

namespace weblayer {

namespace {

bool IsNameValid(const std::string& name) {
  for (size_t i = 0; i < name.size(); ++i) {
    char c = name[i];
    if (!(base::IsAsciiDigit(c) || base::IsAsciiAlpha(c) || c == '_')) {
      return false;
    }
  }
  return true;
}

}  // namespace

class ProfileImpl::DataClearer : public content::BrowsingDataRemover::Observer {
 public:
  DataClearer(content::BrowserContext* browser_context,
              base::OnceCallback<void()> callback)
      : remover_(
            content::BrowserContext::GetBrowsingDataRemover(browser_context)),
        callback_(std::move(callback)) {
    remover_->AddObserver(this);
  }

  ~DataClearer() override { remover_->RemoveObserver(this); }

  void ClearData(int mask, base::Time from_time, base::Time to_time) {
    int origin_types =
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
        content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;
    remover_->RemoveAndReply(from_time, to_time, mask, origin_types, this);
  }

  void OnBrowsingDataRemoverDone() override {
    std::move(callback_).Run();
    delete this;
  }

 private:
  content::BrowsingDataRemover* const remover_;
  base::OnceCallback<void()> callback_;
};

// static
base::FilePath ProfileImpl::GetCachePath(content::BrowserContext* context) {
  DCHECK(context);
  ProfileImpl* profile =
      static_cast<BrowserContextImpl*>(context)->profile_impl();
#if defined(OS_POSIX)
  base::FilePath path;
  {
    base::ScopedAllowBlocking allow_blocking;
    CHECK(base::PathService::Get(base::DIR_CACHE, &path));
    path = path.AppendASCII("profiles").AppendASCII(profile->name_.c_str());
    if (!base::PathExists(path))
      base::CreateDirectory(path);
  }
  return path;
#else
  return profile->data_path_;
#endif
}

ProfileImpl::ProfileImpl(const std::string& name)
    : name_(name),
      download_directory_(BrowserContextImpl::GetDefaultDownloadDirectory()) {
  if (!name.empty()) {
    CHECK(IsNameValid(name));
    {
      base::ScopedAllowBlocking allow_blocking;
      CHECK(base::PathService::Get(DIR_USER_DATA, &data_path_));
      data_path_ = data_path_.AppendASCII("profiles").AppendASCII(name.c_str());

      if (!base::PathExists(data_path_))
        base::CreateDirectory(data_path_);
    }
  }

  // Ensure WebCacheManager is created so that it starts observing
  // OnRenderProcessHostCreated events.
  web_cache::WebCacheManager::GetInstance();

  browser_context_ = std::make_unique<BrowserContextImpl>(this, data_path_);

  locale_change_subscription_ =
      i18n::RegisterLocaleChangeCallback(base::BindRepeating(
          &ProfileImpl::OnLocaleChanged, base::Unretained(this)));
}

ProfileImpl::~ProfileImpl() {
  browser_context_->ShutdownStoragePartitions();
}

content::BrowserContext* ProfileImpl::GetBrowserContext() {
  return browser_context_.get();
}

void ProfileImpl::ClearBrowsingData(
    const std::vector<BrowsingDataType>& data_types,
    base::Time from_time,
    base::Time to_time,
    base::OnceClosure callback) {
  auto* clearer = new DataClearer(browser_context_.get(), std::move(callback));
  // DataClearer will delete itself in OnBrowsingDataRemoverDone().
  // If Profile is destroyed during clearing, it would lead to destroying
  // browser_context_ and then BrowsingDataRemover, which in turn would call
  // OnBrowsingDataRemoverDone(), even though the clearing hasn't been finished.

  int remove_mask = 0;
  // This follows what Chrome does: see browsing_data_bridge.cc.
  for (auto data_type : data_types) {
    switch (data_type) {
      case BrowsingDataType::COOKIES_AND_SITE_DATA:
        remove_mask |= content::BrowsingDataRemover::DATA_TYPE_COOKIES;
        remove_mask |= content::BrowsingDataRemover::DATA_TYPE_DOM_STORAGE;
        remove_mask |= content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES;
        break;
      case BrowsingDataType::CACHE:
        remove_mask |= content::BrowsingDataRemover::DATA_TYPE_CACHE;
        ClearRendererCache();
        break;
      default:
        NOTREACHED();
    }
  }
  clearer->ClearData(remove_mask, from_time, to_time);
}

void ProfileImpl::SetDownloadDirectory(const base::FilePath& directory) {
  download_directory_ = directory;
}

void ProfileImpl::ClearRendererCache() {
  for (content::RenderProcessHost::iterator iter =
           content::RenderProcessHost::AllHostsIterator();
       !iter.IsAtEnd(); iter.Advance()) {
    content::RenderProcessHost* render_process_host = iter.GetCurrentValue();
    if (render_process_host->GetBrowserContext() == browser_context_.get() &&
        render_process_host->IsInitializedAndNotDead()) {
      web_cache::WebCacheManager::GetInstance()->ClearCacheForProcess(
          render_process_host->GetID());
    }
  }
}

void ProfileImpl::OnLocaleChanged() {
  content::BrowserContext::ForEachStoragePartition(
      GetBrowserContext(),
      base::BindRepeating(
          [](const std::string& accept_language,
             content::StoragePartition* storage_partition) {
            storage_partition->GetNetworkContext()->SetAcceptLanguage(
                accept_language);
          },
          i18n::GetAcceptLangs()));
}

std::unique_ptr<Profile> Profile::Create(const std::string& name) {
  return std::make_unique<ProfileImpl>(name);
}

#if defined(OS_ANDROID)
ProfileImpl::ProfileImpl(JNIEnv* env,
                         const base::android::JavaParamRef<jstring>& name)
    : ProfileImpl(ConvertJavaStringToUTF8(env, name)) {}

static jlong JNI_ProfileImpl_CreateProfile(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& name) {
  return reinterpret_cast<jlong>(new ProfileImpl(env, name));
}

static void JNI_ProfileImpl_DeleteProfile(JNIEnv* env, jlong profile) {
  delete reinterpret_cast<ProfileImpl*>(profile);
}

void ProfileImpl::ClearBrowsingData(
    JNIEnv* env,
    const base::android::JavaParamRef<jintArray>& j_data_types,
    const jlong j_from_time_millis,
    const jlong j_to_time_millis,
    const base::android::JavaRef<jobject>& j_callback) {
  std::vector<int> data_type_ints;
  base::android::JavaIntArrayToIntVector(env, j_data_types, &data_type_ints);
  std::vector<BrowsingDataType> data_types;
  data_types.reserve(data_type_ints.size());
  for (int type : data_type_ints) {
    data_types.push_back(static_cast<BrowsingDataType>(type));
  }
  ClearBrowsingData(
      data_types,
      base::Time::FromJavaTime(static_cast<int64_t>(j_from_time_millis)),
      base::Time::FromJavaTime(static_cast<int64_t>(j_to_time_millis)),
      base::BindOnce(base::android::RunRunnableAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
}

void ProfileImpl::SetDownloadDirectory(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& directory) {
  base::FilePath directory_path(
      base::android::ConvertJavaStringToUTF8(directory));

  SetDownloadDirectory(directory_path);
}

#endif  // OS_ANDROID

}  // namespace weblayer
