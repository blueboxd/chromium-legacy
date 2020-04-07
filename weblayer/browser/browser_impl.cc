// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_impl.h"

#include <algorithm>

#include "base/callback_forward.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "components/base32/base32.h"
#include "content/public/browser/browser_context.h"
#include "weblayer/browser/feature_list_creator.h"
#include "weblayer/browser/persistence/browser_persister.h"
#include "weblayer/browser/persistence/minimal_browser_persister.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/common/weblayer_paths.h"
#include "weblayer/public/browser_observer.h"

#if defined(OS_ANDROID)
#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/json/json_writer.h"
#include "weblayer/browser/java/jni/BrowserImpl_jni.h"
#endif

#if defined(OS_ANDROID)
using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
#endif

namespace weblayer {

std::unique_ptr<Browser> Browser::Create(
    Profile* profile,
    const PersistenceInfo* persistence_info) {
  // BrowserImpl's constructor is private.
  auto browser =
      base::WrapUnique(new BrowserImpl(static_cast<ProfileImpl*>(profile)));
  if (persistence_info)
    browser->RestoreStateIfNecessary(*persistence_info);
  return browser;
}

BrowserImpl::~BrowserImpl() {
#if defined(OS_ANDROID)
  // Android side should always remove tabs first (because the Java Tab class
  // owns the C++ Tab). See BrowserImpl.destroy() (in the Java BrowserImpl
  // class).
  DCHECK(tabs_.empty());
#else
  while (!tabs_.empty())
    RemoveTab(tabs_.back().get());
#endif
  profile_->DecrementBrowserImplCount();
}

TabImpl* BrowserImpl::CreateTabForSessionRestore(
    std::unique_ptr<content::WebContents> web_contents,
    const std::string& guid) {
  std::unique_ptr<TabImpl> tab =
      std::make_unique<TabImpl>(profile_, std::move(web_contents), guid);
#if defined(OS_ANDROID)
  Java_BrowserImpl_createTabForSessionRestore(
      AttachCurrentThread(), java_impl_, reinterpret_cast<jlong>(tab.get()));
#endif
  TabImpl* tab_ptr = tab.get();
  AddTab(std::move(tab));
  return tab_ptr;
}

#if defined(OS_ANDROID)
void BrowserImpl::AddTab(JNIEnv* env,
                         const JavaParamRef<jobject>& caller,
                         long native_tab) {
  TabImpl* tab = reinterpret_cast<TabImpl*>(native_tab);
  std::unique_ptr<Tab> owned_tab;
  if (tab->browser())
    owned_tab = tab->browser()->RemoveTab(tab);
  else
    owned_tab.reset(tab);
  AddTab(std::move(owned_tab));
}

void BrowserImpl::RemoveTab(JNIEnv* env,
                            const JavaParamRef<jobject>& caller,
                            long native_tab) {
  // The Java side owns the Tab.
  RemoveTab(reinterpret_cast<TabImpl*>(native_tab)).release();
}

ScopedJavaLocalRef<jobjectArray> BrowserImpl::GetTabs(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  ScopedJavaLocalRef<jclass> clazz =
      base::android::GetClass(env, "org/chromium/weblayer_private/TabImpl");
  jobjectArray tabs = env->NewObjectArray(tabs_.size(), clazz.obj(),
                                          nullptr /* initialElement */);
  base::android::CheckException(env);

  for (size_t i = 0; i < tabs_.size(); ++i) {
    TabImpl* tab = static_cast<TabImpl*>(tabs_[i].get());
    env->SetObjectArrayElement(tabs, i, tab->GetJavaTab().obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, tabs);
}

void BrowserImpl::SetActiveTab(JNIEnv* env,
                               const JavaParamRef<jobject>& caller,
                               long native_tab) {
  SetActiveTab(reinterpret_cast<TabImpl*>(native_tab));
}

ScopedJavaLocalRef<jobject> BrowserImpl::GetActiveTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  if (!active_tab_)
    return nullptr;
  return ScopedJavaLocalRef<jobject>(active_tab_->GetJavaTab());
}

void BrowserImpl::PrepareForShutdown(JNIEnv* env,
                                     const JavaParamRef<jobject>& caller) {
  PrepareForShutdown();
}

ScopedJavaLocalRef<jstring> BrowserImpl::GetPersistenceId(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  return ScopedJavaLocalRef<jstring>(
      base::android::ConvertUTF8ToJavaString(env, GetPersistenceId()));
}

void BrowserImpl::SaveBrowserPersisterIfNecessary(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  browser_persister_->SaveIfNecessary();
}

ScopedJavaLocalRef<jbyteArray> BrowserImpl::GetBrowserPersisterCryptoKey(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  std::vector<uint8_t> key;
  if (browser_persister_)
    key = browser_persister_->GetCryptoKey();
  return base::android::ToJavaByteArray(env, key);
}

ScopedJavaLocalRef<jbyteArray> BrowserImpl::GetMinimalPersistenceState(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  auto state = GetMinimalPersistenceState();
  return base::android::ToJavaByteArray(env, &(state.front()), state.size());
}

void BrowserImpl::RestoreStateIfNecessary(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller,
    const JavaParamRef<jstring>& j_persistence_id,
    const JavaParamRef<jbyteArray>& j_persistence_crypto_key,
    const JavaParamRef<jbyteArray>& j_minimal_persistence_state) {
  Browser::PersistenceInfo persistence_info;
  Browser::PersistenceInfo* persistence_info_ptr = nullptr;

  if (j_persistence_id.obj()) {
    const std::string persistence_id =
        base::android::ConvertJavaStringToUTF8(j_persistence_id);
    if (!persistence_id.empty()) {
      persistence_info.id = persistence_id;
      if (j_persistence_crypto_key.obj()) {
        base::android::JavaByteArrayToByteVector(
            env, j_persistence_crypto_key, &(persistence_info.last_crypto_key));
      }
      persistence_info_ptr = &persistence_info;
    }
  } else if (j_minimal_persistence_state.obj()) {
    base::android::JavaByteArrayToByteVector(env, j_minimal_persistence_state,
                                             &(persistence_info.minimal_state));
    persistence_info_ptr = &persistence_info;
  }
  if (persistence_info_ptr)
    RestoreStateIfNecessary(*persistence_info_ptr);
}

void BrowserImpl::WebPreferencesChanged(JNIEnv* env) {
  for (const auto& tab : tabs_) {
    TabImpl* tab_impl = static_cast<TabImpl*>(tab.get());
    tab_impl->WebPreferencesChanged();
  }
}

void BrowserImpl::OnFragmentStart(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  // FeatureListCreator is created before any Browsers.
  DCHECK(FeatureListCreator::GetInstance());
  FeatureListCreator::GetInstance()->OnBrowserFragmentStarted();
}

#endif

std::vector<uint8_t> BrowserImpl::GetMinimalPersistenceState(
    int max_size_in_bytes) {
  return PersistMinimalState(this, max_size_in_bytes);
}

bool BrowserImpl::GetPasswordEchoEnabled() {
#if defined(OS_ANDROID)
  return Java_BrowserImpl_getPasswordEchoEnabled(AttachCurrentThread(),
                                                 java_impl_);
#else
  return false;
#endif
}

Tab* BrowserImpl::AddTab(std::unique_ptr<Tab> tab) {
  DCHECK(tab);
  TabImpl* tab_impl = static_cast<TabImpl*>(tab.get());
  DCHECK(!tab_impl->browser());
  tabs_.push_back(std::move(tab));
  tab_impl->set_browser(this);
#if defined(OS_ANDROID)
  Java_BrowserImpl_onTabAdded(AttachCurrentThread(), java_impl_,
                              tab_impl->GetJavaTab());
#endif
  for (BrowserObserver& obs : browser_observers_)
    obs.OnTabAdded(tab_impl);
  return tab_impl;
}

std::unique_ptr<Tab> BrowserImpl::RemoveTab(Tab* tab) {
  TabImpl* tab_impl = static_cast<TabImpl*>(tab);
  DCHECK_EQ(this, tab_impl->browser());
  static_cast<TabImpl*>(tab)->set_browser(nullptr);
  auto iter =
      std::find_if(tabs_.begin(), tabs_.end(), base::MatchesUniquePtr(tab));
  DCHECK(iter != tabs_.end());
  std::unique_ptr<Tab> owned_tab = std::move(*iter);
  tabs_.erase(iter);
  const bool active_tab_changed = active_tab_ == tab;
  if (active_tab_changed)
    active_tab_ = nullptr;
#if defined(OS_ANDROID)
  if (active_tab_changed) {
    Java_BrowserImpl_onActiveTabChanged(
        AttachCurrentThread(), java_impl_,
        active_tab_ ? static_cast<TabImpl*>(active_tab_)->GetJavaTab()
                    : nullptr);
  }
  Java_BrowserImpl_onTabRemoved(AttachCurrentThread(), java_impl_,
                                tab ? tab_impl->GetJavaTab() : nullptr);
#endif
  if (active_tab_changed) {
    for (BrowserObserver& obs : browser_observers_)
      obs.OnActiveTabChanged(active_tab_);
  }
  for (BrowserObserver& obs : browser_observers_)
    obs.OnTabRemoved(tab, active_tab_changed);
  return owned_tab;
}

void BrowserImpl::SetActiveTab(Tab* tab) {
  if (GetActiveTab() == tab)
    return;
  // TODO: currently the java side sets visibility, this code likely should
  // too and it should be removed from the java side.
  active_tab_ = static_cast<TabImpl*>(tab);
#if defined(OS_ANDROID)
  Java_BrowserImpl_onActiveTabChanged(
      AttachCurrentThread(), java_impl_,
      active_tab_ ? active_tab_->GetJavaTab() : nullptr);
#endif
  VisibleSecurityStateOfActiveTabChanged();
  for (BrowserObserver& obs : browser_observers_)
    obs.OnActiveTabChanged(active_tab_);
  if (active_tab_)
    active_tab_->web_contents()->GetController().LoadIfNecessary();
}

Tab* BrowserImpl::GetActiveTab() {
  return active_tab_;
}

std::vector<Tab*> BrowserImpl::GetTabs() {
  std::vector<Tab*> tabs(tabs_.size());
  for (size_t i = 0; i < tabs_.size(); ++i)
    tabs[i] = tabs_[i].get();
  return tabs;
}

void BrowserImpl::PrepareForShutdown() {
  browser_persister_.reset();
}

std::string BrowserImpl::GetPersistenceId() {
  return persistence_id_;
}

std::vector<uint8_t> BrowserImpl::GetMinimalPersistenceState() {
  // 0 means use the default max.
  return GetMinimalPersistenceState(0);
}

void BrowserImpl::AddObserver(BrowserObserver* observer) {
  browser_observers_.AddObserver(observer);
}

void BrowserImpl::RemoveObserver(BrowserObserver* observer) {
  browser_observers_.RemoveObserver(observer);
}

BrowserImpl::BrowserImpl(ProfileImpl* profile) : profile_(profile) {
  profile_->IncrementBrowserImplCount();
}

void BrowserImpl::RestoreStateIfNecessary(
    const PersistenceInfo& persistence_info) {
  persistence_id_ = persistence_info.id;
  if (!persistence_id_.empty()) {
    browser_persister_ = std::make_unique<BrowserPersister>(
        GetBrowserPersisterDataPath(), this, persistence_info.last_crypto_key);
  } else if (!persistence_info.minimal_state.empty()) {
    RestoreMinimalState(this, persistence_info.minimal_state);
  }
}

void BrowserImpl::VisibleSecurityStateOfActiveTabChanged() {
  if (visible_security_state_changed_callback_for_tests_)
    std::move(visible_security_state_changed_callback_for_tests_).Run();

#if defined(OS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BrowserImpl_onVisibleSecurityStateOfActiveTabChanged(env, java_impl_);
#endif
}

base::FilePath BrowserImpl::GetBrowserPersisterDataPath() {
  base::FilePath base_path = profile_->GetBrowserPersisterDataBaseDir();
  DCHECK(!GetPersistenceId().empty());
  const std::string encoded_name = base32::Base32Encode(GetPersistenceId());
  return base_path.AppendASCII("State" + encoded_name);
}

#if defined(OS_ANDROID)
// This function is friended. JNI_BrowserImpl_CreateBrowser can not be
// friended, as it requires browser_impl.h to include BrowserImpl_jni.h, which
// is problematic (meaning not really supported and generates compile errors).
BrowserImpl* CreateBrowserForAndroid(ProfileImpl* profile,
                                     const JavaParamRef<jobject>& java_impl) {
  BrowserImpl* browser = new BrowserImpl(profile);
  browser->java_impl_ = java_impl;
  return browser;
}

static jlong JNI_BrowserImpl_CreateBrowser(
    JNIEnv* env,
    jlong profile,
    const JavaParamRef<jobject>& java_impl) {
  // The android side does not trigger restore from the constructor as at the
  // time this is called not enough of WebLayer has been wired up. Specifically,
  // when this is called BrowserImpl.java hasn't obtained the return value so
  // that it can't call any functions and further the client side hasn't been
  // fully created, leading to all sort of assertions if Tabs are created
  // and/or navigations start (which restore may trigger).
  return reinterpret_cast<intptr_t>(CreateBrowserForAndroid(
      reinterpret_cast<ProfileImpl*>(profile), java_impl));
}

static void JNI_BrowserImpl_DeleteBrowser(JNIEnv* env, jlong browser) {
  delete reinterpret_cast<BrowserImpl*>(browser);
}
#endif

}  // namespace weblayer
