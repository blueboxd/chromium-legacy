// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class FontDisplayAutoLCPAlignTestBase : public SimTest {
 public:
  void SetUp() override {
    std::map<std::string, std::string> parameters;
    parameters["intervention-mode"] = intervention_mode_;
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kAlignFontDisplayAutoTimeoutWithLCPGoal, parameters);
    SimTest::SetUp();
  }

  static Vector<char> ReadAhemWoff2() {
    return test::ReadFromFile(test::CoreTestDataPath("Ahem.woff2"))
        ->CopyAs<Vector<char>>();
  }

 protected:
  Element* GetTarget() { return GetDocument().getElementById("target"); }

  const Font& GetTargetFont() {
    return GetTarget()->GetLayoutObject()->Style()->GetFont();
  }

  std::string intervention_mode_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class FontDisplayAutoLCPAlignFailureModeTest
    : public FontDisplayAutoLCPAlignTestBase {
 public:
  void SetUp() override {
    intervention_mode_ = "failure";
    FontDisplayAutoLCPAlignTestBase::SetUp();
  }
};

TEST_F(FontDisplayAutoLCPAlignFailureModeTest, FontFinishesBeforeLCPLimit) {
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest font_resource("https://example.com/Ahem.woff2", "font/woff2");

  LoadURL("https://example.com");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <span id=target style="position:relative">0123456789</span>
  )HTML");

  // The first frame is rendered with invisible fallback, as the web font is
  // still loading, and is in the block display period.
  Compositor().BeginFrame();
  EXPECT_GT(250, GetTarget()->OffsetWidth());
  EXPECT_TRUE(GetTargetFont().ShouldSkipDrawing());

  font_resource.Complete(ReadAhemWoff2());

  // The next frame is rendered with the web font.
  Compositor().BeginFrame();
  EXPECT_EQ(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
}

TEST_F(FontDisplayAutoLCPAlignFailureModeTest, FontFinishesAfterLCPLimit) {
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest font_resource("https://example.com/Ahem.woff2", "font/woff2");

  LoadURL("https://example.com");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <span id=target style="position:relative">0123456789</span>
  )HTML");

  // The first frame is rendered with invisible fallback, as the web font is
  // still loading, and is in the block display period.
  Compositor().BeginFrame();
  EXPECT_GT(250, GetTarget()->OffsetWidth());
  EXPECT_TRUE(GetTargetFont().ShouldSkipDrawing());

  // Wait until we reach the LCP limit, and the relevant timeout fires.
  test::RunDelayedTasks(base::TimeDelta::FromMilliseconds(
      features::kAlignFontDisplayAutoTimeoutWithLCPGoalTimeoutParam.Get()));

  // After reaching the LCP limit, the web font should enter the failure
  // display period. We should render visible fallback for it.
  Compositor().BeginFrame();
  EXPECT_GT(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());

  font_resource.Complete(ReadAhemWoff2());

  // We shouldn't use the web font even if it loads now. It's already in the
  // failure display period.
  Compositor().BeginFrame();
  EXPECT_GT(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
}

TEST_F(FontDisplayAutoLCPAlignFailureModeTest, FontFaceAddedAfterLCPLimit) {
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest font_resource("https://example.com/Ahem.woff2", "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write("<!doctype html>");

  // Wait until we reach the LCP limit, and the relevant timeout fires.
  test::RunDelayedTasks(base::TimeDelta::FromMilliseconds(
      features::kAlignFontDisplayAutoTimeoutWithLCPGoalTimeoutParam.Get()));

  main_resource.Complete(R"HTML(
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <span id=target style="position:relative">0123456789</span>
  )HTML");

  font_resource.Complete(ReadAhemWoff2());

  // Since the font face is added after the LCP limit and is not in the memory
  // cache, we'll treated as already in the failure period to prevent any
  // latency or layout shifting. We should rendering visible fallback for it.
  Compositor().BeginFrame();
  EXPECT_GT(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
}

TEST_F(FontDisplayAutoLCPAlignFailureModeTest,
       FontFaceInMemoryCacheAddedAfterLCPLimit) {
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest font_resource("https://example.com/Ahem.woff2", "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <link rel="preload" as="font" type="font/woff2"
          href="https://example.com/Ahem.woff2" crossorigin>
  )HTML");

  font_resource.Complete(ReadAhemWoff2());

  // Wait until we reach the LCP limit, and the relevant timeout fires.
  test::RunDelayedTasks(base::TimeDelta::FromMilliseconds(
      features::kAlignFontDisplayAutoTimeoutWithLCPGoalTimeoutParam.Get()));

  main_resource.Complete(R"HTML(
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <span id=target style="position:relative">0123456789</span>
  )HTML");

  // The font face is added after the LCP limit, but it's already preloaded and
  // available from the memory cache. We'll render with it as it's immediate
  // available.
  Compositor().BeginFrame();
  EXPECT_EQ(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
}

// https://crbug.com/1065508
TEST_F(FontDisplayAutoLCPAlignFailureModeTest,
       TimeoutFiredAfterDocumentShutdown) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest font_resource("https://example.com/Ahem.woff2", "font/woff2");

  LoadURL("https://example.com");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <span id=target style="position:relative">0123456789</span>
  )HTML");

  font_resource.Finish();

  SimRequest next_page_resource("https://example2.com/", "text/html");
  LoadURL("https://example2.com/");

  // Wait until we reach the LCP limit, and the timeout for the previous
  // document fires. Shouldn't crash here.
  test::RunDelayedTasks(base::TimeDelta::FromMilliseconds(
      features::kAlignFontDisplayAutoTimeoutWithLCPGoalTimeoutParam.Get()));

  next_page_resource.Finish();
}

class FontDisplayAutoLCPAlignSwapModeTest
    : public FontDisplayAutoLCPAlignTestBase {
 public:
  void SetUp() override {
    intervention_mode_ = "swap";
    FontDisplayAutoLCPAlignTestBase::SetUp();
  }
};

TEST_F(FontDisplayAutoLCPAlignSwapModeTest, FontFinishesBeforeLCPLimit) {
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest font_resource("https://example.com/Ahem.woff2", "font/woff2");

  LoadURL("https://example.com");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <span id=target style="position:relative">0123456789</span>
  )HTML");

  // The first frame is rendered with invisible fallback, as the web font is
  // still loading, and is in the block display period.
  Compositor().BeginFrame();
  EXPECT_GT(250, GetTarget()->OffsetWidth());
  EXPECT_TRUE(GetTargetFont().ShouldSkipDrawing());

  font_resource.Complete(ReadAhemWoff2());

  // The next frame is rendered with the web font.
  Compositor().BeginFrame();
  EXPECT_EQ(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
}

TEST_F(FontDisplayAutoLCPAlignSwapModeTest, FontFinishesAfterLCPLimit) {
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest font_resource("https://example.com/Ahem.woff2", "font/woff2");

  LoadURL("https://example.com");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <span id=target style="position:relative">0123456789</span>
  )HTML");

  // The first frame is rendered with invisible fallback, as the web font is
  // still loading, and is in the block display period.
  Compositor().BeginFrame();
  EXPECT_GT(250, GetTarget()->OffsetWidth());
  EXPECT_TRUE(GetTargetFont().ShouldSkipDrawing());

  // Wait until we reach the LCP limit, and the relevant timeout fires.
  test::RunDelayedTasks(base::TimeDelta::FromMilliseconds(
      features::kAlignFontDisplayAutoTimeoutWithLCPGoalTimeoutParam.Get()));

  // After reaching the LCP limit, the web font should enter the swap
  // display period. We should render visible fallback for it.
  Compositor().BeginFrame();
  EXPECT_GT(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());

  font_resource.Complete(ReadAhemWoff2());

  // The web font swaps in after finishing loading.
  Compositor().BeginFrame();
  EXPECT_EQ(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
}

TEST_F(FontDisplayAutoLCPAlignSwapModeTest, FontFaceAddedAfterLCPLimit) {
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest font_resource("https://example.com/Ahem.woff2", "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write("<!doctype html>");

  // Wait until we reach the LCP limit, and the relevant timeout fires.
  test::RunDelayedTasks(base::TimeDelta::FromMilliseconds(
      features::kAlignFontDisplayAutoTimeoutWithLCPGoalTimeoutParam.Get()));

  main_resource.Complete(R"HTML(
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <span id=target style="position:relative">0123456789</span>
  )HTML");

  font_resource.Complete(ReadAhemWoff2());

  // The web font swaps in after finishing loading.
  Compositor().BeginFrame();
  EXPECT_EQ(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
}

TEST_F(FontDisplayAutoLCPAlignSwapModeTest,
       FontFaceInMemoryCacheAddedAfterLCPLimit) {
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest font_resource("https://example.com/Ahem.woff2", "font/woff2");

  LoadURL("https://example.com");
  main_resource.Write(R"HTML(
    <!doctype html>
    <link rel="preload" as="font" type="font/woff2"
          href="https://example.com/Ahem.woff2" crossorigin>
  )HTML");

  font_resource.Complete(ReadAhemWoff2());

  // Wait until we reach the LCP limit, and the relevant timeout fires.
  test::RunDelayedTasks(base::TimeDelta::FromMilliseconds(
      features::kAlignFontDisplayAutoTimeoutWithLCPGoalTimeoutParam.Get()));

  main_resource.Complete(R"HTML(
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
      }
      #target {
        font: 25px/1 custom-font, monospace;
      }
    </style>
    <span id=target style="position:relative">0123456789</span>
  )HTML");

  // The font face is added after the LCP limit, but it's already preloaded and
  // available from the memory cache. We'll render with it as it's immediate
  // available.
  Compositor().BeginFrame();
  EXPECT_EQ(250, GetTarget()->OffsetWidth());
  EXPECT_FALSE(GetTargetFont().ShouldSkipDrawing());
}

}  // namespace blink
