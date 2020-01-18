// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_IMPL_H_
#define WEBLAYER_BROWSER_BROWSER_IMPL_H_

#include <vector>

#include "base/observer_list.h"
#include "build/build_config.h"
#include "weblayer/public/browser.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace weblayer {

class ProfileImpl;
class TabImpl;

class BrowserImpl : public Browser {
 public:
#if defined(OS_ANDROID)
  BrowserImpl(ProfileImpl* profile,
              const base::android::JavaParamRef<jobject>& java_impl);
#endif
  explicit BrowserImpl(ProfileImpl* profile);
  ~BrowserImpl() override;
  BrowserImpl(const BrowserImpl&) = delete;
  BrowserImpl& operator=(const BrowserImpl&) = delete;

  ProfileImpl* profile() { return profile_; }

#if defined(OS_ANDROID)
  void AddTab(JNIEnv* env,
              const base::android::JavaParamRef<jobject>& caller,
              long native_tab);
  void RemoveTab(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& caller,
                 long native_tab);
  base::android::ScopedJavaLocalRef<jobjectArray> GetTabs(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller);
  void SetActiveTab(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& caller,
                    long native_tab);
  base::android::ScopedJavaLocalRef<jobject> GetActiveTab(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller);
#endif

  // Browser:
  void AddTab(Tab* tab) override;
  void RemoveTab(Tab* tab) override;
  void SetActiveTab(Tab* tab) override;
  Tab* GetActiveTab() override;
  const std::vector<Tab*>& GetTabs() override;
  void AddObserver(BrowserObserver* observer) override;
  void RemoveObserver(BrowserObserver* observer) override;

 private:
#if defined(OS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_impl_;
#endif
  base::ObserverList<BrowserObserver> browser_observers_;
  ProfileImpl* profile_;
  std::vector<Tab*> tabs_;
  TabImpl* active_tab_ = nullptr;
  const std::string persistence_id_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_IMPL_H_
