// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NEW_BROWSER_CALLBACK_PROXY_H_
#define WEBLAYER_BROWSER_NEW_BROWSER_CALLBACK_PROXY_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "weblayer/public/new_browser_delegate.h"

namespace weblayer {

class BrowserControllerImpl;

// NewBrowserCallbackProxy forwards all NewBrowserDelegate functions to the Java
// side. There is one NewBrowserCallbackProxy per BrowserController.
class NewBrowserCallbackProxy : public NewBrowserDelegate {
 public:
  NewBrowserCallbackProxy(JNIEnv* env,
                          jobject obj,
                          BrowserControllerImpl* browser_controller);
  ~NewBrowserCallbackProxy() override;

  // NewBrowserDelegate:
  void OnNewBrowser(std::unique_ptr<BrowserController> new_contents,
                    NewBrowserType type) override;

 private:
  BrowserControllerImpl* browser_controller_;
  base::android::ScopedJavaGlobalRef<jobject> java_impl_;

  DISALLOW_COPY_AND_ASSIGN(NewBrowserCallbackProxy);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NEW_BROWSER_CALLBACK_PROXY_H_
