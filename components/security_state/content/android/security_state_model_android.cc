// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/security_state/content/android/jni_headers/SecurityStateModel_jni.h"
#include "components/security_state/content/android/security_state_model_delegate.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/web_contents.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::JavaParamRef;

// static
jint JNI_SecurityStateModel_GetSecurityLevelForWebContents(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    jlong jdelegate) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  SecurityStateModelDelegate* security_state_model_delegate =
      reinterpret_cast<SecurityStateModelDelegate*>(jdelegate);

  if (security_state_model_delegate)
    return security_state_model_delegate->GetSecurityLevel(web_contents);

  return security_state::GetSecurityLevel(
      *security_state::GetVisibleSecurityState(web_contents),
      /* used_policy_installed_certificate= */ false);
}

// static
jboolean JNI_SecurityStateModel_ShouldShowDangerTriangleForWarningLevel(
    JNIEnv* env) {
  return security_state::ShouldShowDangerTriangleForWarningLevel();
}
