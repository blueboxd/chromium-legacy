// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_TEST_INTERFACES_H_
#define CONTENT_SHELL_TEST_RUNNER_TEST_INTERFACES_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/shell/test_runner/test_runner_export.h"

namespace blink {
class WebLocalFrame;
class WebURL;
class WebView;
}

namespace test_runner {
class GamepadController;
class TestRunner;
class WebTestDelegate;
class WebViewTestProxy;

class TEST_RUNNER_EXPORT TestInterfaces {
 public:
  TestInterfaces();
  ~TestInterfaces();

  void SetMainView(blink::WebView* web_view);
  void SetDelegate(WebTestDelegate* delegate);
  void BindTo(blink::WebLocalFrame* frame);
  void ResetTestHelperControllers();
  void ResetAll();
  bool TestIsRunning();
  void SetTestIsRunning(bool running);
  void ConfigureForTestWithURL(const blink::WebURL& test_url,
                               bool protocol_mode);

  void WindowOpened(WebViewTestProxy* proxy);
  void WindowClosed(WebViewTestProxy* proxy);

  TestRunner* GetTestRunner();
  WebTestDelegate* GetDelegate();
  const std::vector<WebViewTestProxy*>& GetWindowList();

 private:
  // Called when a WebTestDelegate is destroyed, if it is the currently used
  // delegate, switch to another delegate in window_list_ as there might be
  // WebFrameTestClients that require it.
  // If window_list_ is empty set delegate_ to nullptr, a new one will be
  // assigned the next time a WebViewTestProxy is built.
  void DelegateDestroyed(WebTestDelegate* delegate);

  std::unique_ptr<GamepadController> gamepad_controller_;
  std::unique_ptr<TestRunner> test_runner_;
  WebTestDelegate* delegate_ = nullptr;

  std::vector<WebViewTestProxy*> window_list_;
  blink::WebView* main_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestInterfaces);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_TEST_INTERFACES_H_
