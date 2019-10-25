// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/renderer/paint_preview_recorder_impl.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace paint_preview {

namespace {

// Checks that |status| == |expected_status| and loads |response| into
// |out_response| if |expected_status| == kOk. If |expected_status| != kOk
// |out_response| can safely be nullptr.
void OnCaptureFinished(mojom::PaintPreviewStatus expected_status,
                       mojom::PaintPreviewCaptureResponsePtr* out_response,
                       mojom::PaintPreviewStatus status,
                       mojom::PaintPreviewCaptureResponsePtr response) {
  EXPECT_EQ(status, expected_status);
  if (expected_status == mojom::PaintPreviewStatus::kOk)
    *out_response = std::move(response);
}

}  // namespace

class PaintPreviewRecorderRenderViewTest : public content::RenderViewTest {
 public:
  PaintPreviewRecorderRenderViewTest() {}
  ~PaintPreviewRecorderRenderViewTest() override {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    RenderViewTest::SetUp();
  }

  content::RenderFrame* GetFrame() { return view_->GetMainRenderFrame(); }

  base::FilePath MakeTestFilePath(const std::string& filename) {
    return temp_dir_.GetPath().AppendASCII(filename);
  }

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(PaintPreviewRecorderRenderViewTest, TestCaptureMainFrame) {
  LoadHTML(
      "<body style='min-height:1000px;'>"
      "  <div style='width: 100px; height: 100px; "
      "              background-color: #000000'>&nbsp;</div>"
      "  <p><a href='https://www.foo.com'>Foo</a></p>"
      "</body>");
  base::FilePath skp_path = MakeTestFilePath("test.skp");

  mojom::PaintPreviewCaptureParamsPtr params =
      mojom::PaintPreviewCaptureParams::New();
  auto token = base::UnguessableToken::Create();
  params->guid = token;
  params->clip_rect = gfx::Rect();
  params->is_main_frame = true;
  base::File skp_file(skp_path,
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  params->file = std::move(skp_file);

  auto out_response = mojom::PaintPreviewCaptureResponse::New();
  content::RenderFrame* frame = GetFrame();
  int routing_id = frame->GetRoutingID();
  PaintPreviewRecorderImpl paint_preview_recorder(frame);
  paint_preview_recorder.CapturePaintPreview(
      std::move(params),
      base::BindOnce(&OnCaptureFinished, mojom::PaintPreviewStatus::kOk,
                     base::Unretained(&out_response)));
  // Here id() is just the routing ID.
  EXPECT_EQ(static_cast<int>(out_response->id), routing_id);
  EXPECT_EQ(out_response->content_id_proxy_id_map.size(), 0U);

  // NOTE: should be non-zero once the Blink implementation is hooked up.
  EXPECT_EQ(out_response->links.size(), 0U);
}

TEST_F(PaintPreviewRecorderRenderViewTest, TestCaptureInvalidFile) {
  LoadHTML("<body></body>");

  mojom::PaintPreviewCaptureParamsPtr params =
      mojom::PaintPreviewCaptureParams::New();
  auto token = base::UnguessableToken::Create();
  params->guid = token;
  params->clip_rect = gfx::Rect();
  params->is_main_frame = true;
  base::File skp_file;  // Invalid file.
  params->file = std::move(skp_file);

  content::RenderFrame* frame = GetFrame();
  PaintPreviewRecorderImpl paint_preview_recorder(frame);
  paint_preview_recorder.CapturePaintPreview(
      std::move(params),
      base::BindOnce(&OnCaptureFinished,
                     mojom::PaintPreviewStatus::kCaptureFailed, nullptr));
}

}  // namespace paint_preview
