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
#include "content/shell/renderer/web_test/test_runner.h"
#include "content/shell/renderer/web_test/text_input_controller.h"
#include "content/shell/renderer/web_test/web_view_test_proxy.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_view.h"

namespace content {

TestInterfaces::TestInterfaces()
    : test_runner_(std::make_unique<TestRunner>()) {
  test_runner_->Reset(nullptr);
}

TestInterfaces::~TestInterfaces() = default;

void TestInterfaces::WindowOpened(WebViewTestProxy* proxy) {
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
      test_runner_->SetMainView(window_list_[0]->GetWebView());
    } else {
      test_runner_->SetMainView(nullptr);
    }
  }
}

}  // namespace content
