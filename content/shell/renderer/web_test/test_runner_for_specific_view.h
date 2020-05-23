// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_WEB_TEST_TEST_RUNNER_FOR_SPECIFIC_VIEW_H_
#define CONTENT_SHELL_RENDERER_WEB_TEST_TEST_RUNNER_FOR_SPECIFIC_VIEW_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "v8/include/v8.h"

namespace blink {
class WebView;
}  // namespace blink

namespace content {
class WebViewTestProxy;

// TestRunnerForSpecificView implements part of |testRunner| javascript bindings
// that work with a view where the javascript call originated from.  Examples:
// - testRunner.capturePixelsAsyncThen
// - testRunner.setPageVisibility
// Note that "global" bindings are handled by TestRunner class.
class TestRunnerForSpecificView {
 public:
  explicit TestRunnerForSpecificView(WebViewTestProxy* web_view_test_proxy);
  ~TestRunnerForSpecificView();

  void Reset();

  // Pointer lock methods used by WebViewTestClient.
  bool RequestPointerLock();
  void RequestPointerUnlock();
  bool isPointerLocked();

 private:
  friend class TestRunnerBindings;

  enum NextPointerLockAction {
    PointerLockWillSucceed,
    PointerLockWillRespondAsync,
    PointerLockWillFailSync,
  };

  void PostTask(base::OnceClosure callback);

  // Pointer lock handling.
  void DidAcquirePointerLock();
  void DidNotAcquirePointerLock();
  void DidLosePointerLock();
  void SetPointerLockWillFailSynchronously();
  void SetPointerLockWillRespondAsynchronously();
  void DidAcquirePointerLockInternal();
  void DidNotAcquirePointerLockInternal();
  void DidLosePointerLockInternal();
  bool pointer_locked_ = false;
  NextPointerLockAction pointer_lock_planned_result_ = PointerLockWillSucceed;

  blink::WebView* web_view();

  WebViewTestProxy* const web_view_test_proxy_;

  base::WeakPtrFactory<TestRunnerForSpecificView> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TestRunnerForSpecificView);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_WEB_TEST_TEST_RUNNER_FOR_SPECIFIC_VIEW_H_
