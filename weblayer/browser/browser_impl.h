// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_IMPL_H_
#define WEBLAYER_BROWSER_BROWSER_IMPL_H_

#include <memory>
#include <vector>

#include "base/observer_list.h"
#include "build/build_config.h"
#include "weblayer/public/browser.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace base {
class FilePath;
}

namespace content {
class WebContents;
}

namespace weblayer {

class ProfileImpl;
class SessionService;
class TabImpl;

class BrowserImpl : public Browser {
 public:
#if defined(OS_ANDROID)
  BrowserImpl(ProfileImpl* profile,
              const PersistenceInfo* persistence_info,
              const base::android::JavaParamRef<jobject>& java_impl);
#endif
  BrowserImpl(ProfileImpl* profile, const PersistenceInfo* persistence_info);
  ~BrowserImpl() override;
  BrowserImpl(const BrowserImpl&) = delete;
  BrowserImpl& operator=(const BrowserImpl&) = delete;

  SessionService* session_service() { return session_service_.get(); }

  ProfileImpl* profile() { return profile_; }

  // Creates and adds a Tab from session restore. The returned tab is owned by
  // this Browser.
  TabImpl* CreateTabForSessionRestore(
      std::unique_ptr<content::WebContents> web_contents);

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
  void PrepareForShutdown(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& caller);
  base::android::ScopedJavaLocalRef<jstring> GetPersistenceId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller);
  void SaveSessionServiceIfNecessary(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller);
  base::android::ScopedJavaLocalRef<jbyteArray> GetSessionServiceCryptoKey(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller);
  base::android::ScopedJavaLocalRef<jbyteArray> GetMinimalPersistenceState(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller);
#endif

  // Used in tests to specify a non-default max (0 means use the default).
  std::vector<uint8_t> GetMinimalPersistenceState(int max_size_in_bytes);

  // Browser:
  Tab* AddTab(std::unique_ptr<Tab> tab) override;
  std::unique_ptr<Tab> RemoveTab(Tab* tab) override;
  void SetActiveTab(Tab* tab) override;
  Tab* GetActiveTab() override;
  std::vector<Tab*> GetTabs() override;
  void PrepareForShutdown() override;
  std::string GetPersistenceId() override;
  std::vector<uint8_t> GetMinimalPersistenceState() override;
  void AddObserver(BrowserObserver* observer) override;
  void RemoveObserver(BrowserObserver* observer) override;

 private:
  void RestoreStateIfNecessary(const PersistenceInfo& persistence_info);

  // Returns the path used by |session_service_|.
  base::FilePath GetSessionServiceDataPath();

#if defined(OS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_impl_;
#endif
  base::ObserverList<BrowserObserver> browser_observers_;
  ProfileImpl* profile_;
  std::vector<std::unique_ptr<Tab>> tabs_;
  TabImpl* active_tab_ = nullptr;
  const std::string persistence_id_;
  std::unique_ptr<SessionService> session_service_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_IMPL_H_
