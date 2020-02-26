// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_WEB_APPLICATIONS_TEST_SANDBOXED_WEB_UI_TEST_BASE_H_
#define CHROMEOS_COMPONENTS_WEB_APPLICATIONS_TEST_SANDBOXED_WEB_UI_TEST_BASE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "chrome/test/base/mojo_web_ui_browser_test.h"

// A base class that can be extended by SWA browser test to inject scripts.
class SandboxedWebUiAppTestBase : public MojoWebUIBrowserTest {
 public:
  SandboxedWebUiAppTestBase(const std::string& host_url,
                            const std::string& sandboxed_url,
                            const base::FilePath& script_path);
  ~SandboxedWebUiAppTestBase() override;

  SandboxedWebUiAppTestBase(const SandboxedWebUiAppTestBase&) = delete;
  SandboxedWebUiAppTestBase& operator=(const SandboxedWebUiAppTestBase&) =
      delete;

  // Returns the contents of the JavaScript library used to help test the
  // sandboxed frame.
  static std::string LoadJsTestLibrary(const base::FilePath& script_path);

  // MojoWebUIBrowserTest:
  void SetUpOnMainThread() override;

 private:
  class TestCodeInjector;

  std::unique_ptr<TestCodeInjector> injector_;
  const std::string host_url_;
  const std::string sandboxed_url_;
  const base::FilePath script_path_;
};

#endif  // CHROMEOS_COMPONENTS_WEB_APPLICATIONS_TEST_SANDBOXED_WEB_UI_TEST_BASE_H_
