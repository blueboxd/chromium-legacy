// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fdio/directory.h>
#include <lib/vfs/cpp/pseudo_dir.h>
#include <lib/vfs/cpp/vmo_file.h>

#include "fuchsia/engine/test/web_engine_browser_test.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/path_service.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
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

// Reads content sourced from VFS PseudoDirs and VmoFiles.
IN_PROC_BROWSER_TEST_F(ContentDirectoryTest, FromVfsPseudoDir) {
  base::ScopedAllowBlockingForTesting allow_block;

  // Get the file contents and store it in a VMO.
  std::string contents;
  base::FilePath pkg_path;
  base::PathService::Get(base::DIR_ASSETS, &pkg_path);
  ASSERT_TRUE(base::ReadFileToString(
      pkg_path.AppendASCII("fuchsia/engine/test/data/title1.html"), &contents));
  zx::vmo contents_vmo;
  zx_status_t status = zx::vmo::create(contents.size(), 0, &contents_vmo);
  ASSERT_EQ(status, ZX_OK);
  status = contents_vmo.write(contents.data(), 0, contents.size());
  ASSERT_EQ(status, ZX_OK);

  // Build up the directory structure of a VmoFile inside a PseudoDir.
  auto vmo_file = std::make_unique<vfs::VmoFile>(
      std::move(contents_vmo), 0, contents.size(),
      vfs::VmoFile::WriteOption::READ_ONLY, vfs::VmoFile::Sharing::CLONE_COW);
  vfs::PseudoDir pseudo_dir;
  status = pseudo_dir.AddEntry("title1.html", std::move(vmo_file));
  ASSERT_EQ(status, ZX_OK);

  // Serve the PseudoDir under the "pseudo-dir" URL.
  fuchsia::web::ContentDirectoryProvider provider;
  provider.set_name("pseudo-dir");
  fidl::InterfaceHandle<fuchsia::io::Directory> directory_channel;
  pseudo_dir.Serve(
      fuchsia::io::OPEN_FLAG_DIRECTORY | fuchsia::io::OPEN_RIGHT_READABLE,
      directory_channel.NewRequest().TakeChannel());
  provider.set_directory(std::move(directory_channel));
  std::vector<fuchsia::web::ContentDirectoryProvider> providers;
  providers.emplace_back(std::move(provider));
  ContentDirectoryLoaderFactory::SetContentDirectoriesForTest(
      std::move(providers));

  // Access the VmoFile under the PseudoDir.
  const GURL kUrl("fuchsia-dir://pseudo-dir/title1.html");
  fuchsia::web::FramePtr frame = CreateFrame();
  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, "title 1");
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
