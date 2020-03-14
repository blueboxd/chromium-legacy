// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/paint_preview/services/paint_preview_demo_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

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

  void SetResponse(mojom::PaintPreviewStatus status) { status_ = status; }

  void BindRequest(mojo::ScopedInterfaceEndpointHandle handle) {
    binding_.Bind(mojo::PendingAssociatedReceiver<mojom::PaintPreviewRecorder>(
        std::move(handle)));
  }

 private:
  mojom::PaintPreviewStatus status_;
  mojo::AssociatedReceiver<mojom::PaintPreviewRecorder> binding_{this};
};

}  // namespace

class PaintPreviewDemoServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  PaintPreviewDemoServiceTest() = default;
  ~PaintPreviewDemoServiceTest() override = default;

  PaintPreviewDemoServiceTest(const PaintPreviewDemoServiceTest&) = delete;
  PaintPreviewDemoServiceTest& operator=(const PaintPreviewDemoServiceTest&) =
      delete;

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    service_ =
        std::make_unique<PaintPreviewDemoService>(temp_dir_.GetPath(), false);
  }

  PaintPreviewDemoService* GetService() { return service_.get(); }

  void OverrideInterface(MockPaintPreviewRecorder* recorder) {
    blink::AssociatedInterfaceProvider* remote_interfaces =
        web_contents()->GetMainFrame()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        mojom::PaintPreviewRecorder::Name_,
        base::BindRepeating(&MockPaintPreviewRecorder::BindRequest,
                            base::Unretained(recorder)));
  }

 private:
  std::unique_ptr<PaintPreviewDemoService> service_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(PaintPreviewDemoServiceTest, CapturePaintPreview) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://www.example.com"));
  const int kTabId = 12U;

  MockPaintPreviewRecorder recorder;
  recorder.SetResponse(mojom::PaintPreviewStatus::kOk);
  OverrideInterface(&recorder);

  auto* service = GetService();
  base::RunLoop loop;
  service->CapturePaintPreview(web_contents(), kTabId,
                               base::BindOnce(
                                   [](base::OnceClosure quit, bool success) {
                                     EXPECT_TRUE(success);
                                     std::move(quit).Run();
                                   },
                                   loop.QuitClosure()));
  loop.Run();

  auto file_manager = service->GetFileManager();
  const DirectoryKey key = file_manager->CreateKey(kTabId);
  service->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_TRUE(exists); }));
  content::RunAllTasksUntilIdle();
}

TEST_F(PaintPreviewDemoServiceTest, CapturePaintPreviewFailed) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://www.example.com"));
  const int kTabId = 12U;

  MockPaintPreviewRecorder recorder;
  recorder.SetResponse(mojom::PaintPreviewStatus::kFailed);
  OverrideInterface(&recorder);

  auto* service = GetService();
  base::RunLoop loop;
  service->CapturePaintPreview(web_contents(), kTabId,
                               base::BindOnce(
                                   [](base::OnceClosure quit, bool success) {
                                     EXPECT_FALSE(success);
                                     std::move(quit).Run();
                                   },
                                   loop.QuitClosure()));
  loop.Run();

  auto file_manager = service->GetFileManager();
  auto key = file_manager->CreateKey(kTabId);
  service->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::DirectoryExists, file_manager, key),
      base::BindOnce([](bool exists) { EXPECT_TRUE(exists); }));
  content::RunAllTasksUntilIdle();
}

}  // namespace paint_preview
