// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/font_preload_manager.h"

#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class FontPreloadManagerTest : public SimTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kFontPreloadingDelaysRendering);
    SimTest::SetUp();
    // We need async parsing for link preloading.
    Document::SetThreadedParsingEnabledForTesting(true);
  }

  void TearDown() override {
    Document::SetThreadedParsingEnabledForTesting(false);
    SimTest::TearDown();
  }

 protected:
  FontPreloadManager& GetFontPreloadManager() {
    return GetDocument().GetFontPreloadManager();
  }

  using State = FontPreloadManager::State;
  State GetState() { return GetFontPreloadManager().state_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FontPreloadManagerTest, FastFontFinishBeforeBody) {
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest font_resource("https://example.com/font.woff", "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <head>
      <link rel="preload" as="font" type="font/woff2"
            href="https://example.com/font.woff">
  )HTML");

  // Set a generous timeout to make sure it doesn't fire before font loads.
  GetFontPreloadManager().SetRenderDelayTimeoutForTest(base::TimeDelta::Max());

  // Run async parsing, which triggers link preloading.
  test::RunPendingTasks();

  // Rendering is blocked due to ongoing font preloading.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_TRUE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kLoading, GetState());

  font_resource.Finish();
  test::RunPendingTasks();

  // Font preloading no longer blocks renderings. However, rendering is still
  // blocked, as we don't have BODY yet.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kLoaded, GetState());

  main_resource.Complete("</head><body>some text</body>");
  test::RunPendingTasks();

  // Rendering starts after BODY has arrived, as the font was loaded earlier.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kUnblocked, GetState());
}

TEST_F(FontPreloadManagerTest, FastFontFinishAfterBody) {
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest font_resource("https://example.com/font.woff", "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <head>
      <link rel="preload" as="font" type="font/woff2"
            href="https://example.com/font.woff">
  )HTML");

  // Set a generous timeout to make sure it doesn't fire before font loads.
  GetFontPreloadManager().SetRenderDelayTimeoutForTest(base::TimeDelta::Max());

  // Run async parsing, which triggers link preloading.
  test::RunPendingTasks();

  // Rendering is blocked due to ongoing font preloading.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_TRUE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kLoading, GetState());

  main_resource.Complete("</head><body>some text</body>");
  test::RunPendingTasks();

  // Rendering is still blocked by font, even if we already have BODY, because
  // the font was *not* loaded earlier.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_TRUE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kLoading, GetState());

  font_resource.Finish();
  test::RunPendingTasks();

  // Rendering starts after font preloading has finished.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kUnblocked, GetState());
}

TEST_F(FontPreloadManagerTest, SlowFontTimeoutBeforeBody) {
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest font_resource("https://example.com/font.woff", "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <head>
      <link rel="preload" as="font" type="font/woff2"
            href="https://example.com/font.woff">
  )HTML");

  // Use a generous timeout to make sure it doesn't automatically fire. We'll
  // manually fire it later.
  GetFontPreloadManager().SetRenderDelayTimeoutForTest(base::TimeDelta::Max());

  // Run async parsing, which triggers link preloading.
  test::RunPendingTasks();

  // Rendering is blocked due to ongoing font preloading.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_TRUE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kLoading, GetState());

  GetFontPreloadManager().FontPreloadingDelaysRenderingTimerFired(nullptr);

  // Font preloading no longer blocks renderings after the timeout fires.
  // However, rendering is still blocked, as we don't have BODY yet.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kUnblocked, GetState());

  main_resource.Complete("</head><body>some text</body>");
  test::RunPendingTasks();

  // Rendering starts after BODY has arrived.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kUnblocked, GetState());

  font_resource.Finish();
}

TEST_F(FontPreloadManagerTest, SlowFontTimeoutAfterBody) {
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest font_resource("https://example.com/font.woff", "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <head>
      <link rel="preload" as="font" type="font/woff2"
            href="https://example.com/font.woff">
  )HTML");

  // Use a generous timeout to make sure it doesn't automatically fire. We'll
  // manually fire it later.
  GetFontPreloadManager().SetRenderDelayTimeoutForTest(base::TimeDelta::Max());

  // Run async parsing, which triggers link preloading.
  test::RunPendingTasks();

  // Rendering is blocked due to ongoing font preloading.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_TRUE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kLoading, GetState());

  main_resource.Complete("</head><body>some text</body>");
  test::RunPendingTasks();

  // Rendering is still blocked by font, even if we already have BODY.
  EXPECT_TRUE(Compositor().DeferMainFrameUpdate());
  EXPECT_TRUE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kLoading, GetState());

  GetFontPreloadManager().FontPreloadingDelaysRenderingTimerFired(nullptr);

  // Rendering starts after we've waited for the font preloading long enough.
  EXPECT_FALSE(Compositor().DeferMainFrameUpdate());
  EXPECT_FALSE(GetFontPreloadManager().HasPendingRenderBlockingFonts());
  EXPECT_EQ(State::kUnblocked, GetState());

  font_resource.Finish();
}

}  // namespace blink
