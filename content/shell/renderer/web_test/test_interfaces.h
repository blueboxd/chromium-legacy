// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_WEB_TEST_TEST_INTERFACES_H_
#define CONTENT_SHELL_RENDERER_WEB_TEST_TEST_INTERFACES_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace content {
class TestRunner;
class WebViewTestProxy;

class TestInterfaces {
 public:
  TestInterfaces();
  ~TestInterfaces();

  void WindowOpened(WebViewTestProxy* proxy);
  void WindowClosed(WebViewTestProxy* proxy);

  TestRunner* GetTestRunner() { return test_runner_.get(); }

 private:
  friend WebViewTestProxy;

  std::unique_ptr<TestRunner> test_runner_;

  std::vector<WebViewTestProxy*> window_list_;

  DISALLOW_COPY_AND_ASSIGN(TestInterfaces);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_WEB_TEST_TEST_INTERFACES_H_
