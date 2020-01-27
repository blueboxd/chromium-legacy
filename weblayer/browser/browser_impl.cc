// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_impl.h"

#include <algorithm>

#include "base/path_service.h"
#include "components/base32/base32.h"
#include "content/public/browser/browser_context.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/session_service.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/common/weblayer_paths.h"
#include "weblayer/public/browser_observer.h"

#if defined(OS_ANDROID)
#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/json/json_writer.h"
#include "weblayer/browser/java/jni/BrowserImpl_jni.h"
#endif

namespace weblayer {

std::unique_ptr<Browser> Browser::Create(Profile* profile,
                                         const std::string& persistence_id) {
  return std::make_unique<BrowserImpl>(static_cast<ProfileImpl*>(profile),
                                       persistence_id);
}

#if defined(OS_ANDROID)
BrowserImpl::BrowserImpl(ProfileImpl* profile,
                         const std::string& persistence_id,
                         const base::android::JavaParamRef<jobject>& java_impl)
    : BrowserImpl(profile, persistence_id) {
  java_impl_ = java_impl;
}
#endif

BrowserImpl::BrowserImpl(ProfileImpl* profile,
                         const std::string& persistence_id)
    : profile_(profile), persistence_id_(persistence_id) {
  if (!persistence_id.empty())
    CreateSessionServiceAndRestore();
}

BrowserImpl::~BrowserImpl() {
  while (!tabs_.empty()) {
    Tab* last_tab = tabs_.back();
    RemoveTab(last_tab);
    DCHECK(!base::Contains(tabs_, last_tab));
  }
}

TabImpl* BrowserImpl::CreateTabForSessionRestore(
    std::unique_ptr<content::WebContents> web_contents) {
  TabImpl* tab = new TabImpl(profile_, std::move(web_contents));
#if defined(OS_ANDROID)
  // The Java side takes ownership of Tab.
  Java_BrowserImpl_createTabForSessionRestore(
      base::android::AttachCurrentThread(), java_impl_,
      reinterpret_cast<jlong>(tab));
#endif
  AddTab(tab);
  return tab;
}

#if defined(OS_ANDROID)
void BrowserImpl::AddTab(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& caller,
                         long native_tab) {
  AddTab(reinterpret_cast<TabImpl*>(native_tab));
}

void BrowserImpl::RemoveTab(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& caller,
                            long native_tab) {
  RemoveTab(reinterpret_cast<TabImpl*>(native_tab));
}

base::android::ScopedJavaLocalRef<jobjectArray> BrowserImpl::GetTabs(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  base::android::ScopedJavaLocalRef<jclass> clazz =
      base::android::GetClass(env, "org/chromium/weblayer_private/TabImpl");
  jobjectArray tabs = env->NewObjectArray(tabs_.size(), clazz.obj(),
                                          nullptr /* initialElement */);
  base::android::CheckException(env);

  for (size_t i = 0; i < tabs_.size(); ++i) {
    TabImpl* tab = static_cast<TabImpl*>(tabs_[i]);
    env->SetObjectArrayElement(tabs, i, tab->GetJavaTab().obj());
  }
  return base::android::ScopedJavaLocalRef<jobjectArray>(env, tabs);
}

void BrowserImpl::SetActiveTab(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    long native_tab) {
  SetActiveTab(reinterpret_cast<TabImpl*>(native_tab));
}

base::android::ScopedJavaLocalRef<jobject> BrowserImpl::GetActiveTab(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  if (!active_tab_)
    return nullptr;
  return base::android::ScopedJavaLocalRef<jobject>(active_tab_->GetJavaTab());
}

void BrowserImpl::PrepareForShutdown(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  PrepareForShutdown();
}

base::android::ScopedJavaLocalRef<jstring> BrowserImpl::GetPersistenceId(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  return base::android::ScopedJavaLocalRef<jstring>(
      base::android::ConvertUTF8ToJavaString(env, persistence_id_));
}

#endif

void BrowserImpl::AddTab(Tab* tab) {
  DCHECK(tab);
  TabImpl* tab_impl = static_cast<TabImpl*>(tab);
  if (tab_impl->browser() != this && tab_impl->browser())
    tab_impl->browser()->RemoveTab(tab);
  tabs_.push_back(tab);
  tab_impl->set_browser(this);
#if defined(OS_ANDROID)
  Java_BrowserImpl_onTabAdded(base::android::AttachCurrentThread(), java_impl_,
                              tab ? tab_impl->GetJavaTab() : nullptr);
#endif
  for (BrowserObserver& obs : browser_observers_)
    obs.OnTabAdded(tab);
}

void BrowserImpl::RemoveTab(Tab* tab) {
  TabImpl* tab_impl = static_cast<TabImpl*>(tab);
  DCHECK_EQ(this, tab_impl->browser());
  static_cast<TabImpl*>(tab)->set_browser(nullptr);
  tabs_.erase(std::find(tabs_.begin(), tabs_.end(), tab));
  const bool active_tab_changed = active_tab_ == tab;
  if (active_tab_changed)
    active_tab_ = nullptr;
#if defined(OS_ANDROID)
  if (active_tab_changed) {
    Java_BrowserImpl_onActiveTabChanged(
        base::android::AttachCurrentThread(), java_impl_,
        active_tab_ ? static_cast<TabImpl*>(active_tab_)->GetJavaTab()
                    : nullptr);
  }
  Java_BrowserImpl_onTabRemoved(base::android::AttachCurrentThread(),
                                java_impl_,
                                tab ? tab_impl->GetJavaTab() : nullptr);
#endif
  if (active_tab_changed) {
    for (BrowserObserver& obs : browser_observers_)
      obs.OnActiveTabChanged(active_tab_);
  }
  for (BrowserObserver& obs : browser_observers_)
    obs.OnTabRemoved(tab, active_tab_changed);
}

void BrowserImpl::SetActiveTab(Tab* tab) {
  if (GetActiveTab() == tab)
    return;
  // TODO: currently the java side sets visibility, this code likely should
  // too and it should be removed from the java side.
  active_tab_ = static_cast<TabImpl*>(tab);
#if defined(OS_ANDROID)
  Java_BrowserImpl_onActiveTabChanged(
      base::android::AttachCurrentThread(), java_impl_,
      active_tab_ ? active_tab_->GetJavaTab() : nullptr);
#endif
  for (BrowserObserver& obs : browser_observers_)
    obs.OnActiveTabChanged(active_tab_);
  if (active_tab_)
    active_tab_->web_contents()->GetController().LoadIfNecessary();
}

Tab* BrowserImpl::GetActiveTab() {
  return active_tab_;
}

const std::vector<Tab*>& BrowserImpl::GetTabs() {
  return tabs_;
}

void BrowserImpl::PrepareForShutdown() {
  session_service_.reset();
}

const std::string& BrowserImpl::GetPersistenceId() {
  return persistence_id_;
}

void BrowserImpl::AddObserver(BrowserObserver* observer) {
  browser_observers_.AddObserver(observer);
}

void BrowserImpl::RemoveObserver(BrowserObserver* observer) {
  browser_observers_.RemoveObserver(observer);
}

base::FilePath BrowserImpl::GetSessionServiceDataPath() {
  base::FilePath base_path;
  if (profile_->GetBrowserContext()->IsOffTheRecord()) {
    CHECK(base::PathService::Get(DIR_USER_DATA, &base_path));
    base_path = base_path.AppendASCII("Incognito Restore Data");
  } else {
    base_path = profile_->data_path().AppendASCII("Restore Data");
  }
  const std::string encoded_name = base32::Base32Encode(persistence_id_);
  return base_path.AppendASCII("State" + encoded_name);
}

void BrowserImpl::CreateSessionServiceAndRestore() {
  session_service_ =
      std::make_unique<SessionService>(GetSessionServiceDataPath(), this);
}

#if defined(OS_ANDROID)
static jlong JNI_BrowserImpl_CreateBrowser(
    JNIEnv* env,
    jlong profile,
    const base::android::JavaParamRef<jstring>& persistence_id,
    const base::android::JavaParamRef<jobject>& java_impl) {
  return reinterpret_cast<intptr_t>(new BrowserImpl(
      reinterpret_cast<ProfileImpl*>(profile),
      persistence_id.obj()
          ? base::android::ConvertJavaStringToUTF8(persistence_id)
          : std::string(),
      java_impl));
}

static void JNI_BrowserImpl_DeleteBrowser(JNIEnv* env, jlong browser) {
  delete reinterpret_cast<BrowserImpl*>(browser);
}
#endif

}  // namespace weblayer
