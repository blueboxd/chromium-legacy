// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/paint_preview/browser/paint_preview_client.h"
#include "components/paint_preview/common/file_stream.h"
#include "components/paint_preview/common/serial_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "url/gurl.h"

namespace paint_preview {

// Test harness for a integration test of paint previews. In this test:
// - Each RenderFrame has an instance of PaintPreviewRecorder attached.
// - Each WebContents has an instance of PaintPreviewClient attached.
// This permits end-to-end testing of the flow of paint previews.
class PaintPreviewBrowserTest : public InProcessBrowserTest {
 protected:
  PaintPreviewBrowserTest() = default;
  ~PaintPreviewBrowserTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    http_server_.ServeFilesFromSourceDirectory("content/test/data");

    ASSERT_TRUE(http_server_.Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
  }

  void CreateClient() {
    PaintPreviewClient::CreateForWebContents(GetWebContents());
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void LoadPage(const GURL& url) {
    ui_test_utils::NavigateToURL(browser(), url);
  }

  void WaitForLoadStopWithoutSuccessCheck() {
    // In many cases, the load may have finished before we get here.  Only wait
    // if the tab still has a pending navigation.
    auto* web_contents = GetWebContents();
    if (web_contents->IsLoading()) {
      content::WindowedNotificationObserver load_stop_observer(
          content::NOTIFICATION_LOAD_STOP,
          content::Source<content::NavigationController>(
              &web_contents->GetController()));
      load_stop_observer.Wait();
    }
  }

  base::ScopedTempDir temp_dir_;
  net::EmbeddedTestServer http_server_;
  net::EmbeddedTestServer http_server_different_origin_;

 private:
  PaintPreviewBrowserTest(const PaintPreviewBrowserTest&) = delete;
  PaintPreviewBrowserTest& operator=(const PaintPreviewBrowserTest&) = delete;
};

IN_PROC_BROWSER_TEST_F(PaintPreviewBrowserTest, CaptureFrame) {
  LoadPage(http_server_.GetURL("a.com", "/cross_site_iframe_factory.html?a"));
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  base::UnguessableToken guid = base::UnguessableToken::Create();
  PaintPreviewClient::PaintPreviewParams params;
  params.document_guid = guid;
  params.is_main_frame = true;
  params.root_dir = temp_dir_.GetPath();
  params.max_per_capture_size = 0;
  base::RunLoop loop;

  CreateClient();
  auto* client = PaintPreviewClient::FromWebContents(GetWebContents());
  WaitForLoadStopWithoutSuccessCheck();
  client->CapturePaintPreview(
      params, GetWebContents()->GetMainFrame(),
      base::BindOnce(
          [](base::RepeatingClosure quit, base::UnguessableToken expected_guid,
             base::UnguessableToken guid, mojom::PaintPreviewStatus status,
             std::unique_ptr<PaintPreviewProto> proto) {
            EXPECT_EQ(guid, expected_guid);
            EXPECT_EQ(status, mojom::PaintPreviewStatus::kOk);
            EXPECT_TRUE(proto->has_root_frame());
            EXPECT_EQ(proto->subframes_size(), 0);
            EXPECT_EQ(proto->root_frame().content_id_to_embedding_tokens_size(),
                      0);
            EXPECT_TRUE(proto->root_frame().is_main_frame());
#if defined(OS_WIN)
            base::FilePath path = base::FilePath(
                base::UTF8ToUTF16(proto->root_frame().file_path()));
#else
            base::FilePath path =
                base::FilePath(proto->root_frame().file_path());
#endif
            {
              base::ScopedAllowBlockingForTesting scoped_blocking;
              EXPECT_TRUE(base::PathExists(path));
              FileRStream rstream(base::File(
                  path, base::File::FLAG_OPEN | base::File::FLAG_READ));
              // Check that the result is a valid SkPicture. Don't bother
              // checking the contents as this could change depending on how
              // page rendering changes and is possibly unstable. nullptr is
              // safe for serial procs as there are no iframes to deserialize.
              EXPECT_NE(SkPicture::MakeFromStream(&rstream, nullptr), nullptr);
            }
            quit.Run();
          },
          loop.QuitClosure(), guid));
  loop.Run();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::PaintPreviewCapture::kEntryName);
  EXPECT_EQ(1u, entries.size());
}

IN_PROC_BROWSER_TEST_F(PaintPreviewBrowserTest, CaptureMainFrameWithSubframe) {
  LoadPage(
      http_server_.GetURL("a.com", "/cross_site_iframe_factory.html?a(b)"));

  base::UnguessableToken guid = base::UnguessableToken::Create();
  PaintPreviewClient::PaintPreviewParams params;
  params.document_guid = guid;
  params.is_main_frame = true;
  params.root_dir = temp_dir_.GetPath();
  params.max_per_capture_size = 0;
  base::RunLoop loop;

  CreateClient();
  auto* client = PaintPreviewClient::FromWebContents(GetWebContents());
  WaitForLoadStopWithoutSuccessCheck();
  client->CapturePaintPreview(
      params, GetWebContents()->GetMainFrame(),
      base::BindOnce(
          [](base::RepeatingClosure quit, base::UnguessableToken expected_guid,
             base::UnguessableToken guid, mojom::PaintPreviewStatus status,
             std::unique_ptr<PaintPreviewProto> proto) {
            EXPECT_EQ(guid, expected_guid);
            EXPECT_EQ(status, mojom::PaintPreviewStatus::kOk);
            EXPECT_TRUE(proto->has_root_frame());
            EXPECT_EQ(proto->subframes_size(), 1);
            EXPECT_EQ(proto->root_frame().content_id_to_embedding_tokens_size(),
                      1);
            EXPECT_TRUE(proto->root_frame().is_main_frame());
            EXPECT_EQ(proto->subframes(0).content_id_to_embedding_tokens_size(),
                      0);
            EXPECT_FALSE(proto->subframes(0).is_main_frame());
#if defined(OS_WIN)
            base::FilePath main_path = base::FilePath(
                base::UTF8ToUTF16(proto->root_frame().file_path()));
            base::FilePath subframe_path = base::FilePath(
                base::UTF8ToUTF16(proto->subframes(0).file_path()));
#else
            base::FilePath main_path =
                base::FilePath(proto->root_frame().file_path());
            base::FilePath subframe_path =
                base::FilePath(proto->subframes(0).file_path());
#endif
            {
              base::ScopedAllowBlockingForTesting scoped_blocking;
              EXPECT_TRUE(base::PathExists(main_path));
              EXPECT_TRUE(base::PathExists(subframe_path));
              FileRStream rstream_main(base::File(
                  main_path, base::File::FLAG_OPEN | base::File::FLAG_READ));
              // Check that the result is a valid SkPicture. Don't bother
              // checking the contents as this could change depending on how
              // page rendering changes and is possibly unstable.
              DeserializationContext ctx;
              auto deserial_procs = MakeDeserialProcs(&ctx);
              EXPECT_NE(
                  SkPicture::MakeFromStream(&rstream_main, &deserial_procs),
                  nullptr);
              FileRStream rstream_subframe(
                  base::File(subframe_path,
                             base::File::FLAG_OPEN | base::File::FLAG_READ));
              ctx.clear();
              deserial_procs = MakeDeserialProcs(&ctx);
              EXPECT_NE(
                  SkPicture::MakeFromStream(&rstream_subframe, &deserial_procs),
                  nullptr);
            }
            quit.Run();
          },
          loop.QuitClosure(), guid));
  loop.Run();
}

}  // namespace paint_preview
