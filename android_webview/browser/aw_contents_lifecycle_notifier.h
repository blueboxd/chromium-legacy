// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_LIFECYCLE_NOTIFIER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_LIFECYCLE_NOTIFIER_H_

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "base/no_destructor.h"

namespace android_webview {

class AwContentsLifecycleNotifier {
 public:
  static void OnWebViewCreated();
  static void OnWebViewDestroyed();

 private:
  friend base::NoDestructor<AwContentsLifecycleNotifier>;

  static AwContentsLifecycleNotifier& getInstance();

  AwContentsLifecycleNotifier();
  ~AwContentsLifecycleNotifier() = delete;

  int mNumWebViews = 0;

  DISALLOW_COPY_AND_ASSIGN(AwContentsLifecycleNotifier);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_LIFECYCLE_NOTIFIER_H_
