// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/profile_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/cookie_manager_impl.h"
#include "weblayer/browser/tab_impl.h"

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

bool g_first_profile_created = false;

// TaskRunner used by MarkProfileAsDeleted and NukeProfilesMarkedForDeletion to
// esnure that Nuke happens before any Mark in this process.
base::SequencedTaskRunner* GetBackgroundDiskOperationTaskRunner() {
  static const base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>>
      task_runner(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
  return task_runner.get()->get();
}

#if defined(OS_ANDROID)
void PassFilePathsToJavaCallback(
    const base::android::ScopedJavaGlobalRef<jobject>& callback,
    const std::vector<std::string>& file_paths) {
  base::android::RunObjectCallbackAndroid(
      callback, base::android::ToJavaArrayOfStrings(
                    base::android::AttachCurrentThread(), file_paths));
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
  return profile->info_.cache_path;
}

ProfileImpl::ProfileImpl(const std::string& name)
    : download_directory_(BrowserContextImpl::GetDefaultDownloadDirectory()) {
  {
    base::ScopedAllowBlocking allow_blocking;
    info_ = CreateProfileInfo(name);
  }

  if (!g_first_profile_created) {
    g_first_profile_created = true;
    GetBackgroundDiskOperationTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&NukeProfilesMarkedForDeletion));
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

  browser_context_ =
      std::make_unique<BrowserContextImpl>(this, info_.data_path);
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

// static
void ProfileImpl::NukeDataAfterRemovingData(
    std::unique_ptr<ProfileImpl> profile,
    base::OnceClosure done_callback) {
  // Need PostTask to avoid reentrancy for deleting |browser_context_|.
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&ProfileImpl::DoNukeData, std::move(profile),
                                std::move(done_callback)));
}

// static
void ProfileImpl::DoNukeData(std::unique_ptr<ProfileImpl> profile,
                             base::OnceClosure done_callback) {
  ProfileInfo info = profile->info_;
  profile.reset();
  GetBackgroundDiskOperationTaskRunner()->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&TryNukeProfileFromDisk, info),
      std::move(done_callback));
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

// static
std::unique_ptr<Profile> Profile::Create(const std::string& name) {
  return std::make_unique<ProfileImpl>(name);
}

// static
std::unique_ptr<Profile> Profile::DestroyAndDeleteDataFromDisk(
    std::unique_ptr<Profile> profile,
    base::OnceClosure done_callback) {
  std::unique_ptr<ProfileImpl> impl(
      static_cast<ProfileImpl*>(profile.release()));
  return ProfileImpl::DestroyAndDeleteDataFromDisk(std::move(impl),
                                                   std::move(done_callback));
}

// static
std::unique_ptr<ProfileImpl> ProfileImpl::DestroyAndDeleteDataFromDisk(
    std::unique_ptr<ProfileImpl> profile,
    base::OnceClosure done_callback) {
  if (profile->num_browser_impl_ > 0)
    return profile;

  GetBackgroundDiskOperationTaskRunner()->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&MarkProfileAsDeleted, profile->info_),
      base::BindOnce(&ProfileImpl::OnProfileMarked, std::move(profile),
                     std::move(done_callback)));
  return nullptr;
}

// static
void ProfileImpl::OnProfileMarked(std::unique_ptr<ProfileImpl> profile,
                                  base::OnceClosure done_callback) {
  // Try to finish all writes and remove all data before nuking the profile.
  static_cast<BrowserContextImpl*>(profile->GetBrowserContext())
      ->pref_service()
      ->CommitPendingWrite();

  // Unretained is safe here because DataClearer is owned by
  // BrowserContextImpl which is owned by this.
  auto* clearer = new DataClearer(
      profile->GetBrowserContext(),
      base::BindOnce(&ProfileImpl::NukeDataAfterRemovingData,
                     std::move(profile), std::move(done_callback)));
  int remove_all_mask = 0x8fffffff;
  clearer->ClearData(remove_all_mask, base::Time::Min(), base::Time::Max());
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

jint ProfileImpl::GetNumBrowserImpl(JNIEnv* env) {
  return num_browser_impl_;
}

jlong ProfileImpl::GetBrowserContext(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(GetBrowserContext());
}

void ProfileImpl::DestroyAndDeleteDataFromDisk(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_completion_callback) {
  std::unique_ptr<ProfileImpl> ptr(this);
  std::unique_ptr<ProfileImpl> result =
      ProfileImpl::DestroyAndDeleteDataFromDisk(
          std::move(ptr),
          base::BindOnce(&base::android::RunRunnableAndroid,
                         base::android::ScopedJavaGlobalRef<jobject>(
                             j_completion_callback)));
  CHECK(!result);
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
  return ComputeBrowserPersisterDataBaseDir(info_);
}

}  // namespace weblayer
