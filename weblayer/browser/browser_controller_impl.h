// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_CONTROLLER_IMPL_H_
#define WEBLAYER_BROWSER_BROWSER_CONTROLLER_IMPL_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "weblayer/public/browser_controller.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace content {
class WebContents;
}

namespace weblayer {
class NavigationControllerImpl;
class ProfileImpl;

#if defined(OS_ANDROID)
class TopControlsContainerView;
#endif

class BrowserControllerImpl : public BrowserController,
                              public content::WebContentsDelegate,
                              public content::WebContentsObserver {
 public:
  explicit BrowserControllerImpl(ProfileImpl* profile);
  ~BrowserControllerImpl() override;

  content::WebContents* web_contents() const { return web_contents_.get(); }

#if defined(OS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetTopControlsContainerView(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      jlong native_top_controls_container_view);
#endif

 private:
  // BrowserController implementation:
  void AddObserver(BrowserObserver* observer) override;
  void RemoveObserver(BrowserObserver* observer) override;
  NavigationController* GetNavigationController() override;
#if !defined(OS_ANDROID)
  void AttachToView(views::WebView* web_view) override;
#endif

  // content::WebContentsDelegate implementation:
  void LoadingStateChanged(content::WebContents* source,
                           bool to_different_document) override;
  void LoadProgressChanged(content::WebContents* source,
                           double progress) override;
  void DidNavigateMainFramePostCommit(
      content::WebContents* web_contents) override;
  int GetTopControlsHeight() override;
  bool DoBrowserControlsShrinkRendererSize(
      const content::WebContents* web_contents) override;

  // content::WebContentsObserver implementation:
  void DidFirstVisuallyNonEmptyPaint() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  ProfileImpl* profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<NavigationControllerImpl> navigation_controller_;
  base::ObserverList<BrowserObserver>::Unchecked observers_;
#if defined(OS_ANDROID)
  TopControlsContainerView* top_controls_container_view_ = nullptr;
#endif

  DISALLOW_COPY_AND_ASSIGN(BrowserControllerImpl);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_CONTROLLER_IMPL_H_
