// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/win/media_foundation_renderer.h"

#include <memory>

#include "media/base/test_data_util.h"
#include "media/test/pipeline_integration_test_base.h"
#include "media/test/test_media_source.h"

namespace media {

class MediaFoundationRendererIntegrationTest
    : public testing::Test,
      public PipelineIntegrationTestBase {
 public:
  MediaFoundationRendererIntegrationTest() {
    SetCreateRendererCB(base::BindRepeating(
        &MediaFoundationRendererIntegrationTest::CreateMediaFoundationRenderer,
        base::Unretained(this)));
  }

 private:
  std::unique_ptr<Renderer> CreateMediaFoundationRenderer(
      base::Optional<RendererFactoryType> factory_type) {
    auto renderer = std::make_unique<MediaFoundationRenderer>(
        /*muted=*/false, task_environment_.GetMainThreadTaskRunner(),
        /*force_dcomp_mode_for_testing=*/true);
    renderer->SetPlaybackElementId(1);  // Must be set before Initialize().
    return renderer;
  }

  DISALLOW_COPY_AND_ASSIGN(MediaFoundationRendererIntegrationTest);
};

TEST_F(MediaFoundationRendererIntegrationTest, BasicPlayback) {
  if (!MediaFoundationRenderer::IsSupported())
    return;

  ASSERT_EQ(PIPELINE_OK, Start("bear-vp9.webm"));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
}

TEST_F(MediaFoundationRendererIntegrationTest, BasicPlayback_MediaSource) {
  if (!MediaFoundationRenderer::IsSupported())
    return;

  TestMediaSource source("bear-vp9.webm", 67504);
  EXPECT_EQ(PIPELINE_OK, StartPipelineWithMediaSource(&source));
  source.EndOfStream();

  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
  source.Shutdown();
  Stop();
}

}  // namespace media
