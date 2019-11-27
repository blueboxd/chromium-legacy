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
#include "weblayer/public/profile.h"

#if defined(OS_ANDROID)
#include <jni.h>
#include "base/android/scoped_java_ref.h"
#endif

namespace content {
class BrowserContext;
}

namespace weblayer {

class ProfileImpl : public Profile {
 public:
  // Return the cache directory path for this BrowserContext. On some
  // platforms, file in cache directory may be deleted by the operating
  // system. So it is suitable for storing data that can be recreated such
  // as caches.
  // |context| must not be null.
  static base::FilePath GetCachePath(content::BrowserContext* context);

  explicit ProfileImpl(const std::string& name);
  ~ProfileImpl() override;

  content::BrowserContext* GetBrowserContext();

  // Profile implementation:
  void ClearBrowsingData(const std::vector<BrowsingDataType>& data_types,
                         base::Time from_time,
                         base::Time to_time,
                         base::OnceClosure callback) override;

#if defined(OS_ANDROID)
  ProfileImpl(JNIEnv* env, const base::android::JavaParamRef<jstring>& path);

  void ClearBrowsingData(
      JNIEnv* env,
      const base::android::JavaParamRef<jintArray>& j_data_types,
      const jlong j_from_time_millis,
      const jlong j_to_time_millis,
      const base::android::JavaRef<jobject>& j_callback);
#endif

 private:
  class BrowserContextImpl;
  class DataClearer;

  void ClearRendererCache();

  // Callback when the system locale has been updated.
  void OnLocaleChanged();

  const std::string name_;
  base::FilePath data_path_;

  std::unique_ptr<BrowserContextImpl> browser_context_;

  std::unique_ptr<i18n::LocaleChangeSubscription> locale_change_subscription_;

  DISALLOW_COPY_AND_ASSIGN(ProfileImpl);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PROFILE_IMPL_H_
