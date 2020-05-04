// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_WEB_TEST_TEST_INTERFACES_H_
#define CONTENT_SHELL_RENDERER_WEB_TEST_TEST_INTERFACES_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace blink {
class WebLocalFrame;
class WebURL;
class WebView;
}  // namespace blink

namespace content {
class BlinkTestRunner;
class GamepadController;
class TestRunner;
class WebViewTestProxy;

class TestInterfaces {
 public:
  TestInterfaces();
  ~TestInterfaces();

  void SetMainView(blink::WebView* web_view);
  void Install(blink::WebLocalFrame* frame);
  void ResetAll();
  bool TestIsRunning();
  void SetTestIsRunning(bool running);
  void ConfigureForTestWithURL(const blink::WebURL& test_url,
                               bool protocol_mode);

  void WindowOpened(WebViewTestProxy* proxy);
  void WindowClosed(WebViewTestProxy* proxy);

  // This returns the BlinkTestRunner from the oldest created WebViewTestProxy.
  // TODO(lukasza): Using the first BlinkTestRunner as the main BlinkTestRunner
  // is wrong, but it is difficult to change because this behavior has been
  // baked for a long time into test assumptions (i.e. which PrintMessage gets
  // delivered to the browser depends on this).
  BlinkTestRunner* GetFirstBlinkTestRunner();

  TestRunner* GetTestRunner();
  // TODO(danakj): This is a list of all RenderViews not of all windows. There
  // will be a RenderView for each frame tree fragment in the process, not just
  // one per window. We should only return the RenderViews with a local main
  // frame.
  // TODO(danakj): Some clients want a list of the main frames (maybe most/all?)
  // so can we add a GetMainFrameList() or something?
  const std::vector<WebViewTestProxy*>& GetWindowList();

 private:
  std::unique_ptr<GamepadController> gamepad_controller_;
  std::unique_ptr<TestRunner> test_runner_;

  std::vector<WebViewTestProxy*> window_list_;
  blink::WebView* main_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestInterfaces);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_WEB_TEST_TEST_INTERFACES_H_
