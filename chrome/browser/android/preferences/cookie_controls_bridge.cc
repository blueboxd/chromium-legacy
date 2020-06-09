// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/cookie_controls_bridge.h"

#include <memory>

#include "chrome/android/chrome_jni_headers/CookieControlsBridge_jni.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/embedder_support/android/browser_context/browser_context_handle.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/browser_context.h"

using base::android::JavaParamRef;

CookieControlsBridge::CookieControlsBridge(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jweb_contents_android,
    const base::android::JavaParamRef<jobject>&
        joriginal_browser_context_handle)
    : jobject_(obj) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents_android);
  content::BrowserContext* context =
      browser_context::BrowserContextFromJavaHandle(
          joriginal_browser_context_handle);
  controller_ =
      std::make_unique<CookieControlsController>(web_contents, context);
  observer_.Add(controller_.get());
  controller_->Update(web_contents);
}

void CookieControlsBridge::OnStatusChanged(
    CookieControlsStatus new_status,
    CookieControlsEnforcement new_enforcement,
    int blocked_cookies) {
  if (status_ != new_status || enforcement_ != new_enforcement) {
    status_ = new_status;
    enforcement_ = new_enforcement;
    JNIEnv* env = base::android::AttachCurrentThread();
    // Only call status callback if status has changed
    Java_CookieControlsBridge_onCookieBlockingStatusChanged(
        env, jobject_, static_cast<int>(status_),
        static_cast<int>(enforcement_));
  }

  OnBlockedCookiesCountChanged(blocked_cookies);
}

void CookieControlsBridge::OnBlockedCookiesCountChanged(int blocked_cookies) {
  // The blocked cookie count changes quite frequently, so avoid unnecessary
  // UI updates if possible.
  if (blocked_cookies_ == blocked_cookies)
    return;

  blocked_cookies_ = blocked_cookies;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CookieControlsBridge_onBlockedCookiesCountChanged(
      env, jobject_, blocked_cookies_.value_or(0));
}

void CookieControlsBridge::SetThirdPartyCookieBlockingEnabledForSite(
    JNIEnv* env,
    bool block_cookies) {
  controller_->OnCookieBlockingEnabledForSite(block_cookies);
}

void CookieControlsBridge::OnUiClosing(JNIEnv* env) {
  controller_->OnUiClosing();
}

CookieControlsBridge::~CookieControlsBridge() = default;

void CookieControlsBridge::Destroy(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  delete this;
}

jboolean JNI_CookieControlsBridge_IsCookieControlsEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jbrowser_context_handle) {
  content::BrowserContext* context =
      browser_context::BrowserContextFromJavaHandle(jbrowser_context_handle);
  return permissions::PermissionsClient::Get()
      ->GetCookieSettings(context)
      ->IsCookieControlsEnabled();
}

static jlong JNI_CookieControlsBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jweb_contents_android,
    const base::android::JavaParamRef<jobject>&
        joriginal_browser_context_handle) {
  return reinterpret_cast<intptr_t>(new CookieControlsBridge(
      env, obj, jweb_contents_android, joriginal_browser_context_handle));
}
