// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PROFILE_IMPL_H_
#define WEBLAYER_BROWSER_PROFILE_IMPL_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "weblayer/browser/i18n_util.h"
#include "weblayer/browser/profile_disk_operations.h"
#include "weblayer/public/profile.h"

#if defined(OS_ANDROID)
#include <jni.h>
#include "base/android/scoped_java_ref.h"
#endif

namespace content {
class BrowserContext;
}

namespace weblayer {
class BrowserContextImpl;
class CookieManagerImpl;

class ProfileImpl : public Profile {
 public:
  // Return the cache directory path for this BrowserContext. On some
  // platforms, file in cache directory may be deleted by the operating
  // system. So it is suitable for storing data that can be recreated such
  // as caches.
  // |context| must not be null.
  static base::FilePath GetCachePath(content::BrowserContext* context);

  static std::unique_ptr<ProfileImpl> DestroyAndDeleteDataFromDisk(
      std::unique_ptr<ProfileImpl> profile,
      base::OnceClosure done_callback);

  explicit ProfileImpl(const std::string& name);
  ~ProfileImpl() override;

  // Returns the ProfileImpl from the specified BrowserContext.
  static ProfileImpl* FromBrowserContext(
      content::BrowserContext* browser_context);

  content::BrowserContext* GetBrowserContext();

  // Called when the download subsystem has finished initializing. By this point
  // information about downloads that were interrupted by a previous crash would
  // be available.
  void DownloadsInitialized();

  // Path data is stored at, empty if off-the-record.
  const base::FilePath& data_path() const { return info_.data_path; }
  DownloadDelegate* download_delegate() { return download_delegate_; }

  // Profile implementation:
  void ClearBrowsingData(const std::vector<BrowsingDataType>& data_types,
                         base::Time from_time,
                         base::Time to_time,
                         base::OnceClosure callback) override;
  void SetDownloadDirectory(const base::FilePath& directory) override;
  void SetDownloadDelegate(DownloadDelegate* delegate) override;
  CookieManager* GetCookieManager() override;

#if defined(OS_ANDROID)
  ProfileImpl(JNIEnv* env,
              const base::android::JavaParamRef<jstring>& path,
              const base::android::JavaParamRef<jobject>& java_profile);

  jint GetNumBrowserImpl(JNIEnv* env);
  jlong GetBrowserContext(JNIEnv* env);
  void DestroyAndDeleteDataFromDisk(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& j_completion_callback);
  void ClearBrowsingData(
      JNIEnv* env,
      const base::android::JavaParamRef<jintArray>& j_data_types,
      const jlong j_from_time_millis,
      const jlong j_to_time_millis,
      const base::android::JavaRef<jobject>& j_callback);
  void SetDownloadDirectory(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& directory);
  jlong GetCookieManager(JNIEnv* env);
  void EnsureBrowserContextInitialized(JNIEnv* env);
#endif

  void IncrementBrowserImplCount();
  void DecrementBrowserImplCount();
  const base::FilePath& download_directory() { return download_directory_; }

  // Get the directory where BrowserPersister stores tab state data. This will
  // be a real file path even for the off-the-record profile.
  base::FilePath GetBrowserPersisterDataBaseDir() const;

 private:
  class DataClearer;

  static void OnProfileMarked(std::unique_ptr<ProfileImpl> profile,
                              base::OnceClosure done_callback);
  static void NukeDataAfterRemovingData(std::unique_ptr<ProfileImpl> profile,
                                        base::OnceClosure done_callback);
  static void DoNukeData(std::unique_ptr<ProfileImpl> profile,
                         base::OnceClosure done_callback);
  void ClearRendererCache();

  // Callback when the system locale has been updated.
  void OnLocaleChanged();

  ProfileInfo info_;

  std::unique_ptr<BrowserContextImpl> browser_context_;

  base::FilePath download_directory_;

  DownloadDelegate* download_delegate_ = nullptr;

  std::unique_ptr<i18n::LocaleChangeSubscription> locale_change_subscription_;

  std::unique_ptr<CookieManagerImpl> cookie_manager_;

  size_t num_browser_impl_ = 0u;

#if defined(OS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_profile_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ProfileImpl);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PROFILE_IMPL_H_
