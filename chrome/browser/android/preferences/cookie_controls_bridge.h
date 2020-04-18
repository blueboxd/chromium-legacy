// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PREFERENCES_COOKIE_CONTROLS_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_PREFERENCES_COOKIE_CONTROLS_BRIDGE_H_

#include "base/android/jni_weak_ref.h"
#include "base/optional.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_controller.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_view.h"
#include "components/content_settings/core/common/cookie_controls_status.h"

// Communicates between CookieControlsController (C++ backend) and PageInfoView
// (Java UI).
class CookieControlsBridge : public CookieControlsView {
 public:
  // Creates a CookeControlsBridge for interaction with a
  // CookieControlsController.
  CookieControlsBridge(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jweb_contents_android);

  ~CookieControlsBridge() override;

  // Called by the Java counterpart when it is getting garbage collected.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void SetThirdPartyCookieBlockingEnabledForSite(JNIEnv* env,
                                                 bool block_cookies);

  void OnUiClosing(JNIEnv* env);

  // CookieControlsView:
  void OnStatusChanged(CookieControlsStatus status,
                       CookieControlsEnforcement enforcement,
                       int blocked_cookies) override;
  void OnBlockedCookiesCountChanged(int blocked_cookies) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> jobject_;
  CookieControlsStatus status_ = CookieControlsStatus::kUninitialized;
  CookieControlsEnforcement enforcement_ =
      CookieControlsEnforcement::kNoEnforcement;
  base::Optional<int> blocked_cookies_;
  std::unique_ptr<CookieControlsController> controller_;
  ScopedObserver<CookieControlsController, CookieControlsView> observer_{this};

  DISALLOW_COPY_AND_ASSIGN(CookieControlsBridge);
};

#endif  // CHROME_BROWSER_ANDROID_PREFERENCES_COOKIE_CONTROLS_BRIDGE_H_
