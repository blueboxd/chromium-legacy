// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/SingleTabModel_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/webapps/single_tab_mode_tab_helper.h"
#include "content/public/browser/web_contents.h"

using base::android::JavaParamRef;

static void JNI_SingleTabModel_PermanentlyBlockAllNewWindows(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_tab_android) {
  TabAndroid* tab = TabAndroid::GetNativeTab(env, j_tab_android);
  if (!tab)
    return;

  content::WebContents* web_contents = tab->web_contents();
  if (!web_contents)
    return;

  SingleTabModeTabHelper* tab_helper =
      SingleTabModeTabHelper::FromWebContents(web_contents);
  tab_helper->PermanentlyBlockAllNewWindows();
}
