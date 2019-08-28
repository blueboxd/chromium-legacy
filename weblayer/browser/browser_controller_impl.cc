// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_controller_impl.h"

#include "base/logging.h"
#include "content/public/browser/web_contents.h"
#include "weblayer/browser/navigation_controller_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/public/browser_observer.h"

#if !defined(OS_ANDROID)
#include "ui/views/controls/webview/webview.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "weblayer/browser/java/jni/BrowserControllerImpl_jni.h"
#endif

namespace weblayer {

BrowserControllerImpl::BrowserControllerImpl(ProfileImpl* profile,
                                             const gfx::Size& initial_size)
    : profile_(profile) {
  content::WebContents::CreateParams create_params(
      profile_->GetBrowserContext());
  create_params.initial_size = initial_size;
  web_contents_ = content::WebContents::Create(create_params);

  web_contents_->SetDelegate(this);
  Observe(web_contents_.get());

  navigation_controller_ = std::make_unique<NavigationControllerImpl>(this);
}

BrowserControllerImpl::~BrowserControllerImpl() {
  // Destruct this now to avoid it calling back when this object is partially
  // destructed.
  web_contents_.reset();
}

void BrowserControllerImpl::AddObserver(BrowserObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserControllerImpl::RemoveObserver(BrowserObserver* observer) {
  observers_.RemoveObserver(observer);
}

NavigationController* BrowserControllerImpl::GetNavigationController() {
  return navigation_controller_.get();
}

#if !defined(OS_ANDROID)
void BrowserControllerImpl::AttachToView(views::WebView* web_view) {
  web_view->SetWebContents(web_contents_.get());
  web_contents_->Focus();
}
#endif

#if defined(OS_ANDROID)
static jlong JNI_BrowserControllerImpl_CreateBrowserController(JNIEnv* env,
                                                               jlong profile) {
  // TODO: figure out size.
  return reinterpret_cast<intptr_t>(new BrowserControllerImpl(
      reinterpret_cast<ProfileImpl*>(profile), gfx::Size()));
}

static void JNI_BrowserControllerImpl_DeleteBrowserController(
    JNIEnv* env,
    jlong browser_controller) {
  delete reinterpret_cast<BrowserControllerImpl*>(browser_controller);
}

base::android::ScopedJavaLocalRef<jobject>
BrowserControllerImpl::GetWebContents(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return web_contents_->GetJavaWebContents();
}

void BrowserControllerImpl::Navigate(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& path) {
  GetNavigationController()->Navigate(
      GURL(base::android::ConvertJavaStringToUTF8(env, path)));
}
#endif

void BrowserControllerImpl::LoadingStateChanged(content::WebContents* source,
                                                bool to_different_document) {
  bool is_loading = web_contents_->IsLoading();
  for (auto& observer : observers_)
    observer.LoadingStateChanged(is_loading, to_different_document);
}

void BrowserControllerImpl::DidNavigateMainFramePostCommit(
    content::WebContents* web_contents) {
  for (auto& observer : observers_)
    observer.DisplayedURLChanged(web_contents->GetVisibleURL());
}

void BrowserControllerImpl::DidFirstVisuallyNonEmptyPaint() {
  for (auto& observer : observers_)
    observer.FirstContentfulPaint();
}

std::unique_ptr<BrowserController> BrowserController::Create(
    Profile* profile,
    const gfx::Size& initial_size) {
  return std::make_unique<BrowserControllerImpl>(
      static_cast<ProfileImpl*>(profile), initial_size);
}

}  // namespace weblayer
