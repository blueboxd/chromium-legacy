// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_FULLSCREEN_DELEGATE_PROXY_H_
#define WEBLAYER_BROWSER_FULLSCREEN_DELEGATE_PROXY_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "weblayer/public/fullscreen_delegate.h"

namespace weblayer {

class BrowserController;

// FullscreenDelegateProxy forwards all FullscreenDelegate functions to the
// Java side. There is at most one FullscreenDelegateProxy per
// BrowserController.
class FullscreenDelegateProxy : public FullscreenDelegate {
 public:
  FullscreenDelegateProxy(JNIEnv* env,
                          jobject obj,
                          BrowserController* browser_controller);
  ~FullscreenDelegateProxy() override;

  // FullscreenDelegate:
  void EnterFullscreen(base::OnceClosure exit_closure) override;
  void ExitFullscreen() override;

  // Called from the Java side to exit fullscreen.
  void DoExitFullscreen(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& caller);

 private:
  BrowserController* browser_controller_;
  base::android::ScopedJavaGlobalRef<jobject> java_observer_;
  base::OnceClosure exit_fullscreen_closure_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenDelegateProxy);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_FULLSCREEN_DELEGATE_PROXY_H_
