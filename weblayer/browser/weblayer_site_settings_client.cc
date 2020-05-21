// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/java/jni/WebLayerSiteSettingsClient_jni.h"

#include "components/content_settings/core/common/pref_names.h"
#include "components/embedder_support/android/browser_context/browser_context_handle.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

using base::android::JavaParamRef;

namespace weblayer {

namespace {

PrefService* GetPrefService(
    const JavaParamRef<jobject>& jbrowser_context_handle) {
  content::BrowserContext* browser_context =
      browser_context::BrowserContextFromJavaHandle(jbrowser_context_handle);
  if (!browser_context)
    return nullptr;
  return user_prefs::UserPrefs::Get(browser_context);
}

}  // namespace

static jboolean JNI_WebLayerSiteSettingsClient_GetBlockThirdPartyCookies(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle) {
  PrefService* pref_service = GetPrefService(jbrowser_context_handle);
  if (!pref_service)
    return false;
  return pref_service->GetBoolean(prefs::kBlockThirdPartyCookies);
}

static void JNI_WebLayerSiteSettingsClient_SetBlockThirdPartyCookies(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    jboolean value) {
  PrefService* pref_service = GetPrefService(jbrowser_context_handle);
  if (!pref_service)
    return;
  pref_service->SetBoolean(prefs::kBlockThirdPartyCookies, value);
}

static jint JNI_WebLayerSiteSettingsClient_GetCookieControlsMode(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle) {
  PrefService* pref_service = GetPrefService(jbrowser_context_handle);
  if (!pref_service)
    return 0;
  return pref_service->GetInteger(prefs::kCookieControlsMode);
}

static void JNI_WebLayerSiteSettingsClient_SetCookieControlsMode(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    jint value) {
  PrefService* pref_service = GetPrefService(jbrowser_context_handle);
  if (!pref_service)
    return;
  pref_service->SetInteger(prefs::kCookieControlsMode, value);
}

}  // namespace weblayer
