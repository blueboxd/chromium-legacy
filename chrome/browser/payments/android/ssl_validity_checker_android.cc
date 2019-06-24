// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_java_ref.h"
#include "chrome/android/chrome_jni_headers/SslValidityChecker_jni.h"
#include "chrome/browser/payments/ssl_validity_checker.h"
#include "content/public/browser/web_contents.h"

namespace payments {

// static
jboolean JNI_SslValidityChecker_IsSslCertificateValid(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  return SslValidityChecker::IsSslCertificateValid(
      content::WebContents::FromJavaWebContents(jweb_contents));
}

// static
jboolean JNI_SslValidityChecker_IsValidPageInPaymentHandlerWindow(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  return SslValidityChecker::IsValidPageInPaymentHandlerWindow(
      content::WebContents::FromJavaWebContents(jweb_contents));
}

}  // namespace payments
