// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "chrome/android/chrome_jni_headers/SecurityStateModel_jni.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "url/gurl.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::JavaParamRef;

// static
jint JNI_SecurityStateModel_GetSecurityLevelForWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  SecurityStateTabHelper::CreateForWebContents(web_contents);
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  DCHECK(helper);
  return helper->GetSecurityLevel();
}

// static
jboolean JNI_SecurityStateModel_IsSchemeCryptographic(
    JNIEnv* env,
    const JavaParamRef<jstring>& jurl) {
  GURL url(ConvertJavaStringToUTF16(env, jurl));
  return url.is_valid() && url.SchemeIsCryptographic();
}

// static
jboolean JNI_SecurityStateModel_ShouldDowngradeNeutralStyling(
    JNIEnv* env,
    jint jsecurity_level,
    const JavaParamRef<jstring>& jurl) {
  GURL url(ConvertJavaStringToUTF16(env, jurl));
  auto security_level =
      static_cast<security_state::SecurityLevel>(jsecurity_level);
  return security_state::ShouldDowngradeNeutralStyling(
      security_level, url, base::BindRepeating(&content::IsOriginSecure));
}
