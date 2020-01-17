// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_contents_lifecycle_notifier.h"

#include "android_webview/browser_jni_headers/AwContentsLifecycleNotifier_jni.h"
#include "content/public/browser/browser_thread.h"

using base::android::AttachCurrentThread;
using content::BrowserThread;

namespace android_webview {

// static
void AwContentsLifecycleNotifier::OnWebViewCreated() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (getInstance().mNumWebViews++ == 0) {
    Java_AwContentsLifecycleNotifier_onFirstWebViewCreated(
        AttachCurrentThread());
  }
}

// static
void AwContentsLifecycleNotifier::OnWebViewDestroyed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (--getInstance().mNumWebViews == 0) {
    Java_AwContentsLifecycleNotifier_onLastWebViewDestroyed(
        AttachCurrentThread());
  }
}

// static
AwContentsLifecycleNotifier& AwContentsLifecycleNotifier::getInstance() {
  static base::NoDestructor<AwContentsLifecycleNotifier> instance;
  return *instance;
}

AwContentsLifecycleNotifier::AwContentsLifecycleNotifier() = default;

}  // namespace android_webview
