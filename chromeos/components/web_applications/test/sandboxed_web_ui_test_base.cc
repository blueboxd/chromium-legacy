// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/web_applications/test/sandboxed_web_ui_test_base.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

class SandboxedWebUiAppTestBase::TestCodeInjector
    : public content::TestNavigationObserver {
 public:
  TestCodeInjector(const std::string& host_url,
                   const std::string& sandboxed_url,
                   const base::FilePath& script_path)
      : TestNavigationObserver(GURL(host_url)),
        sandboxed_url_(sandboxed_url),
        script_path_(script_path) {
    WatchExistingWebContents();
    StartWatchingNewWebContents();
  }

  // TestNavigationObserver:
  void OnDidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetURL().GetOrigin() != GURL(sandboxed_url_))
      return;

    auto* render_frame_host = navigation_handle->GetRenderFrameHost();

    // Use ExecuteScript(), not ExecJs(), because of Content Security Policy
    // directive: "script-src chrome://resources 'self'"
    ASSERT_TRUE(content::ExecuteScript(render_frame_host,
                                       LoadJsTestLibrary(script_path_)));
    TestNavigationObserver::OnDidFinishNavigation(navigation_handle);
  }

 private:
  const std::string sandboxed_url_;
  const base::FilePath script_path_;
};

SandboxedWebUiAppTestBase::SandboxedWebUiAppTestBase(
    const std::string& host_url,
    const std::string& sandboxed_url,
    const base::FilePath& script_path)
    : host_url_(host_url),
      sandboxed_url_(sandboxed_url),
      script_path_(script_path) {}

SandboxedWebUiAppTestBase::~SandboxedWebUiAppTestBase() = default;

// static
std::string SandboxedWebUiAppTestBase::LoadJsTestLibrary(
    const base::FilePath& script_path) {
  base::FilePath source_root;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root));
  const auto full_script_path = source_root.Append(script_path);

  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string injected_content;
  EXPECT_TRUE(base::ReadFileToString(full_script_path, &injected_content));
  return injected_content;
}

void SandboxedWebUiAppTestBase::SetUpOnMainThread() {
  injector_ = std::make_unique<TestCodeInjector>(host_url_, sandboxed_url_,
                                                 script_path_);
  MojoWebUIBrowserTest::SetUpOnMainThread();
}
