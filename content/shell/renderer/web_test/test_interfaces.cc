// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/web_test/test_interfaces.h"

#include <stddef.h>

#include <string>

#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/shell/renderer/web_test/gamepad_controller.h"
#include "content/shell/renderer/web_test/test_runner.h"
#include "content/shell/renderer/web_test/text_input_controller.h"
#include "content/shell/renderer/web_test/web_view_test_proxy.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_testing_support.h"
#include "third_party/blink/public/web/web_view.h"

namespace content {

TestInterfaces::TestInterfaces()
    : gamepad_controller_(new GamepadController()),
      test_runner_(new TestRunner(this)) {
  // NOTE: please don't put feature specific enable flags here,
  // instead add them to runtime_enabled_features.json5.
  //
  // Stores state to be restored after each test.
  blink::WebTestingSupport::SaveRuntimeFeatures();
}

TestInterfaces::~TestInterfaces() {
  // gamepad_controller_ doesn't depend on WebView.
  test_runner_->SetMainView(nullptr);

  // gamepad_controller_ ignores SetDelegate(nullptr)
  test_runner_->SetDelegate(nullptr);
}

void TestInterfaces::SetMainView(blink::WebView* web_view) {
  // gamepad_controller_ doesn't depend on WebView.
  main_view_ = web_view;
  test_runner_->SetMainView(web_view);
}

void TestInterfaces::Install(blink::WebLocalFrame* frame) {
  gamepad_controller_->Install(frame);
}

void TestInterfaces::ResetAll() {
  blink::WebTestingSupport::ResetRuntimeFeatures();
  blink::WebCache::Clear();

  gamepad_controller_->Reset();

  for (WebViewTestProxy* web_view_test_proxy : window_list_)
    web_view_test_proxy->Reset();
  test_runner_->Reset();
}

bool TestInterfaces::TestIsRunning() {
  return test_runner_->TestIsRunning();
}

void TestInterfaces::SetTestIsRunning(bool running) {
  test_runner_->SetTestIsRunning(running);
}

void TestInterfaces::ConfigureForTestWithURL(const blink::WebURL& test_url,
                                             bool protocol_mode) {
  std::string spec = GURL(test_url).spec();
  size_t path_start = spec.rfind("web_tests/");
  if (path_start != std::string::npos)
    spec = spec.substr(path_start);

  bool is_devtools_test =
      spec.find("/devtools/") != std::string::npos ||
      spec.find("/inspector-protocol/") != std::string::npos;
  if (is_devtools_test)
    test_runner_->SetDumpConsoleMessages(false);

  // In protocol mode (see TestInfo::protocol_mode), we dump layout only when
  // requested by the test. In non-protocol mode, we dump layout by default
  // because the layout may be the only interesting thing to the user while
  // we don't dump non-human-readable binary data. In non-protocol mode, we
  // still generate pixel results (though don't dump them) to let the renderer
  // execute the same code regardless of the protocol mode, e.g. for ease of
  // debugging a web test issue.
  if (!protocol_mode)
    test_runner_->SetShouldDumpAsLayout(true);

  // For http/tests/loading/, which is served via httpd and becomes /loading/.
  if (spec.find("/loading/") != std::string::npos)
    test_runner_->SetShouldDumpFrameLoadCallbacks(true);
  if (spec.find("/dumpAsText/") != std::string::npos) {
    test_runner_->SetShouldDumpAsText(true);
    test_runner_->SetShouldGeneratePixelResults(false);
  }
  test_runner_->SetV8CacheDisabled(is_devtools_test);

  if (spec.find("/viewsource/") != std::string::npos) {
    test_runner_->SetShouldEnableViewSource(true);
    test_runner_->SetShouldGeneratePixelResults(false);
    test_runner_->SetShouldDumpAsMarkup(true);
  }
  if (spec.find("/external/wpt/") != std::string::npos ||
      spec.find("/external/csswg-test/") != std::string::npos ||
      spec.find("://web-platform.test") != std::string::npos ||
      spec.find("/harness-tests/wpt/") != std::string::npos)
    test_runner_->SetIsWebPlatformTestsMode();
}

void TestInterfaces::WindowOpened(WebViewTestProxy* proxy) {
  if (window_list_.empty()) {
    // The first WebViewTestProxy in |window_list_| provides the
    // BlinkTestRunner.
    // TODO(lukasza): Using the first BlinkTestRunner as the main
    // BlinkTestRunner is wrong, but it is difficult to change because this
    // behavior has been baked for a long time into test assumptions (i.e. which
    // PrintMessage gets  delivered to the browser depends on this).
    test_runner_->SetDelegate(proxy->blink_test_runner());
  }
  window_list_.push_back(proxy);
}

void TestInterfaces::WindowClosed(WebViewTestProxy* proxy) {
  std::vector<WebViewTestProxy*>::iterator pos =
      std::find(window_list_.begin(), window_list_.end(), proxy);
  if (pos == window_list_.end()) {
    NOTREACHED();
    return;
  }

  const bool was_first = window_list_[0] == proxy;

  window_list_.erase(pos);

  // If this was the first WebViewTestProxy, we replace the pointer
  // to the new "first" WebViewTestProxy. If there's no WebViewTestProxy
  // at all, then we'll set it when one is created.
  // TODO(lukasza): Using the first BlinkTestRunner as the main BlinKTestRunner
  // is wrong, but it is difficult to change because this behavior has been
  // baked for a long time into test assumptions (i.e. which PrintMessage gets
  // delivered to the browser depends on this).
  if (was_first) {
    if (!window_list_.empty()) {
      test_runner_->SetDelegate(window_list_[0]->blink_test_runner());
      SetMainView(window_list_[0]->GetWebView());
    } else {
      test_runner_->SetDelegate(nullptr);
      SetMainView(nullptr);
    }
  }
}

TestRunner* TestInterfaces::GetTestRunner() {
  return test_runner_.get();
}

BlinkTestRunner* TestInterfaces::GetFirstBlinkTestRunner() {
  return window_list_[0]->blink_test_runner();
}

const std::vector<WebViewTestProxy*>& TestInterfaces::GetWindowList() {
  return window_list_;
}

}  // namespace content
