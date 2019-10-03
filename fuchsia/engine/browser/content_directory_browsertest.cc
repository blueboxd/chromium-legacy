// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fdio/directory.h>

#include "fuchsia/engine/test/web_engine_browser_test.h"

#include "base/files/file_path.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/path_service.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_timeouts.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "fuchsia/engine/browser/content_directory_loader_factory.h"
#include "fuchsia/engine/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ContentDirectoryTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  ContentDirectoryTest()
      : run_timeout_(TestTimeouts::action_timeout(),
                     base::MakeExpectedNotRunClosure(FROM_HERE)) {}
  ~ContentDirectoryTest() override = default;

  void SetUp() override {
    // Set this flag early so that the fuchsia-dir:// scheme will be
    // registered at browser startup.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kContentDirectories);

    cr_fuchsia::WebEngineBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    std::vector<fuchsia::web::ContentDirectoryProvider> providers;

    fuchsia::web::ContentDirectoryProvider provider;
    provider.set_name("testdata");
    base::FilePath pkg_path;
    base::PathService::Get(base::DIR_ASSETS, &pkg_path);
    provider.set_directory(
        OpenDirectoryHandle(pkg_path.AppendASCII("fuchsia/engine/test/data")));
    providers.emplace_back(std::move(provider));

    provider = {};
    provider.set_name("alternate");
    provider.set_directory(
        OpenDirectoryHandle(pkg_path.AppendASCII("fuchsia/engine/test/data")));
    providers.emplace_back(std::move(provider));

    ContentDirectoryLoaderFactory::SetContentDirectoriesForTest(
        std::move(providers));

    cr_fuchsia::WebEngineBrowserTest::SetUpOnMainThread();
  }

  void TearDown() override {
    ContentDirectoryLoaderFactory::SetContentDirectoriesForTest({});
  }

  fidl::InterfaceHandle<fuchsia::io::Directory> OpenDirectoryHandle(
      const base::FilePath& path) {
    zx::channel directory_channel;
    zx::channel remote_directory_channel;
    zx_status_t status =
        zx::channel::create(0, &directory_channel, &remote_directory_channel);
    ZX_DCHECK(status == ZX_OK, status) << "zx_channel_create";

    status = fdio_open(
        path.value().c_str(),
        fuchsia::io::OPEN_FLAG_DIRECTORY | fuchsia::io::OPEN_RIGHT_READABLE,
        remote_directory_channel.release());
    ZX_DCHECK(status == ZX_OK, status) << "fdio_open";

    return fidl::InterfaceHandle<fuchsia::io::Directory>(
        std::move(directory_channel));
  }

 protected:
  // Creates a Frame with |navigation_listener_| attached.
  fuchsia::web::FramePtr CreateFrame() {
    return WebEngineBrowserTest::CreateFrame(&navigation_listener_);
  }

  cr_fuchsia::TestNavigationListener navigation_listener_;

 private:
  const base::RunLoop::ScopedRunTimeoutForTest run_timeout_;

  DISALLOW_COPY_AND_ASSIGN(ContentDirectoryTest);
};

IN_PROC_BROWSER_TEST_F(ContentDirectoryTest, Navigate) {
  const GURL kUrl("fuchsia-dir://testdata/title1.html");

  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  navigation_listener_.RunUntilUrlEquals(kUrl);
}

// Navigate to a resource stored under a secondary provider.
IN_PROC_BROWSER_TEST_F(ContentDirectoryTest, NavigateAlternate) {
  const GURL kUrl("fuchsia-dir://alternate/title1.html");

  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  navigation_listener_.RunUntilUrlEquals(kUrl);
}

IN_PROC_BROWSER_TEST_F(ContentDirectoryTest, ScriptSubresource) {
  const GURL kUrl("fuchsia-dir://testdata/include_script.html");

  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "title set by script");
}

IN_PROC_BROWSER_TEST_F(ContentDirectoryTest, ImgSubresource) {
  const GURL kUrl("fuchsia-dir://testdata/include_image.html");

  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "image fetched");
}

// Verify that resource providers are origin-isolated.
IN_PROC_BROWSER_TEST_F(ContentDirectoryTest, ScriptSrcCrossOriginBlocked) {
  const GURL kUrl("fuchsia-dir://testdata/cross_origin_include_script.html");

  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  // If the cross-origin script succeeded, then we should see "title set by
  // script". If "not clobbered" remains set, then we know that CROS enforcement
  // is working.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "same origin ftw");
}

IN_PROC_BROWSER_TEST_F(ContentDirectoryTest, CrossOriginImgBlocked) {
  const GURL kUrl("fuchsia-dir://testdata/cross_origin_include_image.html");

  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));

  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "image rejected");
}

IN_PROC_BROWSER_TEST_F(ContentDirectoryTest, MetadataFileParsed) {
  const GURL kUrl("fuchsia-dir://testdata/mime_override.html");

  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(
      kUrl, "content-type: text/bleep; charset=US-ASCII");
}

IN_PROC_BROWSER_TEST_F(ContentDirectoryTest, BadMetadataFile) {
  const GURL kUrl("fuchsia-dir://testdata/mime_override_invalid.html");

  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl,
                                                 "content-type: text/html");
}

}  // namespace
