// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/android/paint_preview_utils.h"

#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/zlib/google/zip.h"

namespace paint_preview {

namespace {

class MockPaintPreviewRecorder : public mojom::PaintPreviewRecorder {
 public:
  MockPaintPreviewRecorder() = default;
  ~MockPaintPreviewRecorder() override = default;

  MockPaintPreviewRecorder(const MockPaintPreviewRecorder&) = delete;
  MockPaintPreviewRecorder& operator=(const MockPaintPreviewRecorder&) = delete;

  void CapturePaintPreview(
      mojom::PaintPreviewCaptureParamsPtr params,
      mojom::PaintPreviewRecorder::CapturePaintPreviewCallback callback)
      override {
    std::move(callback).Run(status_, mojom::PaintPreviewCaptureResponse::New());
  }

  void SetResponseStatus(mojom::PaintPreviewStatus status) { status_ = status; }

  void BindRequest(mojo::ScopedInterfaceEndpointHandle handle) {
    binding_.Bind(mojo::PendingAssociatedReceiver<mojom::PaintPreviewRecorder>(
        std::move(handle)));
  }

 private:
  mojom::PaintPreviewStatus status_;
  mojo::AssociatedReceiver<mojom::PaintPreviewRecorder> binding_{this};
};

base::Optional<base::FilePath> Unzip(const base::FilePath& zip) {
  base::FilePath dst_path = zip.RemoveExtension();
  if (!base::CreateDirectory(dst_path))
    return base::nullopt;
  if (!zip::Unzip(zip, dst_path))
    return base::nullopt;
  base::DeleteFileRecursively(zip);
  return dst_path;
}

}  // namespace

class PaintPreviewUtilsRenderViewHostTest
    : public content::RenderViewHostTestHarness {
 public:
  PaintPreviewUtilsRenderViewHostTest() {}

  PaintPreviewUtilsRenderViewHostTest(
      const PaintPreviewUtilsRenderViewHostTest&) = delete;
  PaintPreviewUtilsRenderViewHostTest& operator=(
      const PaintPreviewUtilsRenderViewHostTest&) = delete;

 protected:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
    ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
  }

  void OverrideInterface(MockPaintPreviewRecorder* service) {
    blink::AssociatedInterfaceProvider* remote_interfaces =
        web_contents()->GetMainFrame()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        mojom::PaintPreviewRecorder::Name_,
        base::BindRepeating(&MockPaintPreviewRecorder::BindRequest,
                            base::Unretained(service)));
  }
};

TEST_F(PaintPreviewUtilsRenderViewHostTest, CaptureSingleFrameAndKeep) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://www.example.com"));
  auto* contents = content::WebContents::FromRenderFrameHost(main_rfh());
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  MockPaintPreviewRecorder service;
  service.SetResponseStatus(mojom::PaintPreviewStatus::kOk);
  OverrideInterface(&service);

  base::RunLoop loop;
  Capture(contents,
          base::BindOnce(
              [](base::OnceClosure quit,
                 const base::Optional<base::FilePath>& maybe_zip_path) {
                EXPECT_TRUE(maybe_zip_path.has_value());

                const base::FilePath& zip_path = maybe_zip_path.value();
                EXPECT_EQ(".zip", zip_path.Extension());
                {
                  base::ScopedAllowBlockingForTesting scope;
                  EXPECT_TRUE(base::PathExists(zip_path));
                  auto unzipped_path = Unzip(zip_path);
                  EXPECT_TRUE(unzipped_path.has_value());

                  base::FileEnumerator enumerate(unzipped_path.value(), false,
                                                 base::FileEnumerator::FILES);
                  size_t count = 0;
                  bool has_proto = false;
                  bool has_skp = false;
                  for (base::FilePath name = enumerate.Next(); !name.empty();
                       name = enumerate.Next(), ++count) {
                    if (name.Extension() == ".skp")
                      has_skp = true;
                    if (name.BaseName().AsUTF8Unsafe() == "proto.pb")
                      has_proto = true;
                  }
                  EXPECT_EQ(2U, count);
                  EXPECT_TRUE(has_skp);
                  EXPECT_TRUE(has_proto);
                  base::DeleteFileRecursively(zip_path.DirName());
                }
                std::move(quit).Run();
              },
              loop.QuitClosure()),
          true);
  loop.Run();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::PaintPreviewCapture::kEntryName);
  EXPECT_EQ(2U, entries.size());
}

TEST_F(PaintPreviewUtilsRenderViewHostTest, CaptureSingleFrameAndDelete) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://www.example.com"));
  auto* contents = content::WebContents::FromRenderFrameHost(main_rfh());
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  MockPaintPreviewRecorder service;
  service.SetResponseStatus(mojom::PaintPreviewStatus::kOk);
  OverrideInterface(&service);

  base::RunLoop loop;
  Capture(contents,
          base::BindOnce(
              [](base::OnceClosure quit,
                 const base::Optional<base::FilePath>& maybe_zip_path) {
                EXPECT_TRUE(maybe_zip_path.has_value());
                const base::FilePath& zip_path = maybe_zip_path.value();
                {
                  base::ScopedAllowBlockingForTesting scope;
                  EXPECT_FALSE(base::PathExists(zip_path));
                  EXPECT_FALSE(base::DirectoryExists(zip_path.DirName()));
                }
                std::move(quit).Run();
              },
              loop.QuitClosure()),
          false);
  loop.Run();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::PaintPreviewCapture::kEntryName);
  EXPECT_EQ(2U, entries.size());
}

TEST_F(PaintPreviewUtilsRenderViewHostTest, SingleFrameFailure) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://www.example.com"));
  auto* contents = content::WebContents::FromRenderFrameHost(main_rfh());
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  MockPaintPreviewRecorder service;
  service.SetResponseStatus(mojom::PaintPreviewStatus::kFailed);
  OverrideInterface(&service);

  base::RunLoop loop;
  Capture(contents,
          base::BindOnce(
              [](base::OnceClosure quit,
                 const base::Optional<base::FilePath>& path) {
                EXPECT_FALSE(path.has_value());
                std::move(quit).Run();
              },
              loop.QuitClosure()),
          false);
  loop.Run();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::PaintPreviewCapture::kEntryName);
  EXPECT_EQ(0U, entries.size());
}

}  // namespace paint_preview
