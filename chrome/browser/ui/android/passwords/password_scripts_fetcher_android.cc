// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "chrome/android/chrome_jni_headers/PasswordScriptsFetcherBridge_jni.h"
#include "chrome/browser/password_manager/password_scripts_fetcher_factory.h"
#include "components/embedder_support/android/browser_context/browser_context_handle.h"
#include "components/password_manager/core/browser/password_scripts_fetcher.h"
#include "content/public/browser/browser_context.h"

namespace password_manager {

// static
void JNI_PasswordScriptsFetcherBridge_PrewarmCache(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jbrowser_context) {
  content::BrowserContext* context =
      browser_context::BrowserContextFromJavaHandle(jbrowser_context);

  DCHECK(context);
  PasswordScriptsFetcherFactory::GetInstance()
      ->GetForBrowserContext(context)
      ->PrewarmCache();
}

}  // namespace password_manager
