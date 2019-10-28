// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PROFILE_IMPL_H_
#define WEBLAYER_BROWSER_PROFILE_IMPL_H_

#include "build/build_config.h"
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
  explicit ProfileImpl(const base::FilePath& path);
  ~ProfileImpl() override;

  content::BrowserContext* GetBrowserContext();

  // Profile implementation:
  void ClearBrowsingData() override;

#if defined(OS_ANDROID)
  ProfileImpl(JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_profile,
      const base::android::JavaParamRef<jstring>& path);

  void ClearBrowsingData(JNIEnv* env) { ClearBrowsingData(); }
#endif

 private:
  class BrowserContextImpl;
  class DataClearer;

  void OnBrowsingDataCleared();

  base::FilePath path_;
  std::unique_ptr<BrowserContextImpl> browser_context_;
  std::unique_ptr<DataClearer> data_clearer_;

#if defined(OS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_profile_;
#endif
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PROFILE_IMPL_H_
