// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_OBSERVER_PROXY_H_
#define WEBLAYER_BROWSER_BROWSER_OBSERVER_PROXY_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "weblayer/public/browser_observer.h"

namespace weblayer {

class BrowserController;

// BrowserObserverProxy forwards all BrowserObserver functions to the Java
// side. There is one BrowserObserverProxy per BrowserController.
class BrowserObserverProxy : public BrowserObserver {
 public:
  BrowserObserverProxy(JNIEnv* env,
                       jobject obj,
                       BrowserController* browser_controller);
  ~BrowserObserverProxy() override;

  // BrowserObserver:
  void DisplayedURLChanged(const GURL& url) override;
  void LoadingStateChanged(bool is_loading,
                           bool to_different_document) override;
  void LoadProgressChanged(double progress) override;
  void FirstContentfulPaint() override;

 private:
  BrowserController* browser_controller_;
  base::android::ScopedJavaGlobalRef<jobject> java_observer_;

  DISALLOW_COPY_AND_ASSIGN(BrowserObserverProxy);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_OBSERVER_PROXY_H_
