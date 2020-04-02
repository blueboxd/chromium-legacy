// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/profile_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
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
#include "weblayer/browser/cookie_manager_impl.h"
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

// Return the data path directory to profiles.
base::FilePath GetProfileRootDataDir() {
  base::FilePath path;
  CHECK(base::PathService::Get(DIR_USER_DATA, &path));
  return path.AppendASCII("profiles");
}

#if defined(OS_POSIX)
base::FilePath ComputeCachePath(const std::string& profile_name) {
  base::FilePath path;
  CHECK(base::PathService::Get(base::DIR_CACHE, &path));
  return path.AppendASCII("profiles").AppendASCII(profile_name.c_str());
}
#endif  // OS_POSIX

base::FilePath ComputeBrowserPersisterDataBaseDir(
    const base::FilePath& data_path) {
  base::FilePath base_path;
  if (data_path.empty()) {
    CHECK(base::PathService::Get(DIR_USER_DATA, &base_path));
    base_path = base_path.AppendASCII("Incognito Restore Data");
  } else {
    base_path = data_path.AppendASCII("Restore Data");
  }
  return base_path;
}

void NukeProfileFromDisk(const std::string& profile_name,
                         const base::FilePath& data_path) {
  if (data_path.empty()) {
    // Incognito. Just delete session data.
    base::DeleteFileRecursively(ComputeBrowserPersisterDataBaseDir(data_path));
    return;
  }

  base::DeleteFileRecursively(data_path);
#if defined(OS_POSIX)
  base::DeleteFileRecursively(ComputeCachePath(profile_name));
#endif
}

#if defined(OS_ANDROID)
// Returned path only contains the directory name.
// Invalid profile names are ignored.
std::vector<base::FilePath> ListProfileNames() {
  base::FilePath root_dir = GetProfileRootDataDir();
  std::vector<base::FilePath> profile_names;
  base::FileEnumerator enumerator(root_dir, /*recursive=*/false,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    base::FilePath name = enumerator.GetInfo().GetName();
    if (IsNameValid(name.MaybeAsASCII())) {
      profile_names.push_back(name);
    }
  }
  return profile_names;
}

void PassFilePathsToJavaCallback(
    const base::android::ScopedJavaGlobalRef<jobject>& callback,
    const std::vector<base::FilePath>& file_paths) {
  std::vector<std::string> strings;
  for (const auto& path : file_paths) {
    strings.push_back(path.value());
  }
  base::android::RunObjectCallbackAndroid(
      callback, base::android::ToJavaArrayOfStrings(
                    base::android::AttachCurrentThread(), strings));
}
#endif  // OS_ANDROID

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
  ProfileImpl* profile = FromBrowserContext(context);
#if defined(OS_POSIX)
  base::FilePath path = ComputeCachePath(profile->name_);
  {
    base::ScopedAllowBlocking allow_blocking;
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
      data_path_ = GetProfileRootDataDir().AppendASCII(name.c_str());

      if (!base::PathExists(data_path_))
        base::CreateDirectory(data_path_);
    }
  }

  // Ensure WebCacheManager is created so that it starts observing
  // OnRenderProcessHostCreated events.
  web_cache::WebCacheManager::GetInstance();
}

ProfileImpl::~ProfileImpl() {
  DCHECK_EQ(num_browser_impl_, 0u);
  if (browser_context_)
    browser_context_->ShutdownStoragePartitions();
}

ProfileImpl* ProfileImpl::FromBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<BrowserContextImpl*>(browser_context)->profile_impl();
}

content::BrowserContext* ProfileImpl::GetBrowserContext() {
  if (browser_context_)
    return browser_context_.get();

  browser_context_ = std::make_unique<BrowserContextImpl>(this, data_path_);
  locale_change_subscription_ =
      i18n::RegisterLocaleChangeCallback(base::BindRepeating(
          &ProfileImpl::OnLocaleChanged, base::Unretained(this)));
  return browser_context_.get();
}

void ProfileImpl::DownloadsInitialized() {
#if defined(OS_ANDROID)
  return Java_ProfileImpl_downloadsInitialized(
      base::android::AttachCurrentThread(), java_profile_);
#endif
}

bool ProfileImpl::DeleteDataFromDisk(base::OnceClosure done_callback) {
  if (num_browser_impl_ > 0)
    return false;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&NukeProfileFromDisk, name_, data_path_),
      std::move(done_callback));
  return true;
}

void ProfileImpl::ClearBrowsingData(
    const std::vector<BrowsingDataType>& data_types,
    base::Time from_time,
    base::Time to_time,
    base::OnceClosure callback) {
  auto* clearer = new DataClearer(GetBrowserContext(), std::move(callback));
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

void ProfileImpl::SetDownloadDelegate(DownloadDelegate* delegate) {
  download_delegate_ = delegate;
}

CookieManager* ProfileImpl::GetCookieManager() {
  if (!cookie_manager_)
    cookie_manager_ = std::make_unique<CookieManagerImpl>(GetBrowserContext());
  return cookie_manager_.get();
}

void ProfileImpl::ClearRendererCache() {
  for (content::RenderProcessHost::iterator iter =
           content::RenderProcessHost::AllHostsIterator();
       !iter.IsAtEnd(); iter.Advance()) {
    content::RenderProcessHost* render_process_host = iter.GetCurrentValue();
    if (render_process_host->GetBrowserContext() == GetBrowserContext() &&
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
ProfileImpl::ProfileImpl(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& name,
    const base::android::JavaParamRef<jobject>& java_profile)
    : ProfileImpl(ConvertJavaStringToUTF8(env, name)) {
  java_profile_ = java_profile;
}

static jlong JNI_ProfileImpl_CreateProfile(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& name,
    const base::android::JavaParamRef<jobject>& java_profile) {
  return reinterpret_cast<jlong>(new ProfileImpl(env, name, java_profile));
}

static void JNI_ProfileImpl_DeleteProfile(JNIEnv* env, jlong profile) {
  delete reinterpret_cast<ProfileImpl*>(profile);
}

static void JNI_ProfileImpl_EnumerateAllProfileNames(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ListProfileNames),
      base::BindOnce(&PassFilePathsToJavaCallback,
                     base::android::ScopedJavaGlobalRef<jobject>(callback)));
}

jboolean ProfileImpl::DeleteDataFromDisk(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_completion_callback) {
  return DeleteDataFromDisk(base::BindOnce(
      &base::android::RunRunnableAndroid,
      base::android::ScopedJavaGlobalRef<jobject>(j_completion_callback)));
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

jlong ProfileImpl::GetCookieManager(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetCookieManager());
}

void ProfileImpl::EnsureBrowserContextInitialized(JNIEnv* env) {
  content::BrowserContext::GetDownloadManager(GetBrowserContext());
}
#endif  // OS_ANDROID

void ProfileImpl::IncrementBrowserImplCount() {
  num_browser_impl_++;
}

void ProfileImpl::DecrementBrowserImplCount() {
  DCHECK_GT(num_browser_impl_, 0u);
  num_browser_impl_--;
}

base::FilePath ProfileImpl::GetBrowserPersisterDataBaseDir() const {
  return ComputeBrowserPersisterDataBaseDir(data_path_);
}

}  // namespace weblayer
