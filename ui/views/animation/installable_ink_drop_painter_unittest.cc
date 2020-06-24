// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/installable_ink_drop_painter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/animation/installable_ink_drop_config.h"

namespace views {

namespace {

class InstallableInkDropPainterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.base_color = SK_ColorCYAN;
    config_.ripple_opacity = 1.0f;
    config_.highlight_opacity = 1.0f;

    state_.flood_fill_center = gfx::PointF(5.0f, 5.0f);
    state_.flood_fill_progress = 1.0f;
    state_.highlighted_ratio = 0.0f;
  }

  InstallableInkDropConfig config_;
  InstallableInkDropPainter::State state_;
};

}  // namespace

TEST_F(InstallableInkDropPainterTest, MinSize) {
  InstallableInkDropPainter painter(&config_, &state_);
  EXPECT_EQ(gfx::Size(), painter.GetMinimumSize());
}

TEST_F(InstallableInkDropPainterTest, Paint) {
  InstallableInkDropPainter painter(&config_, &state_);

  {
    // No highlight, half filled.
    state_.flood_fill_progress = 0.5f;
    gfx::Canvas canvas(gfx::Size(10, 10), 1.0f, true);
    SkBitmap bitmap = canvas.GetBitmap();
    painter.Paint(&canvas, gfx::Size(5.0f, 5.0f));
    EXPECT_EQ(SkColorSetA(SK_ColorBLACK, 0), bitmap.getColor(0, 0));
    EXPECT_EQ(SK_ColorCYAN, bitmap.getColor(4, 4));
  }

  {
    // No highlight, fully filled.
    state_.flood_fill_progress = 1.0f;
    gfx::Canvas canvas(gfx::Size(10, 10), 1.0f, true);
    SkBitmap bitmap = canvas.GetBitmap();
    painter.Paint(&canvas, gfx::Size(5.0f, 5.0f));
    EXPECT_EQ(SK_ColorCYAN, bitmap.getColor(0, 0));
    EXPECT_EQ(SK_ColorCYAN, bitmap.getColor(4, 4));
  }

  {
    // Use highlight, half filled.
    state_.flood_fill_progress = 0.5f;
    state_.highlighted_ratio = 0.5f;
    gfx::Canvas canvas(gfx::Size(10, 10), 1.0f, true);
    SkBitmap bitmap = canvas.GetBitmap();
    painter.Paint(&canvas, gfx::Size(5.0f, 5.0f));
    EXPECT_EQ(0x7F007F7Fu, bitmap.getColor(1, 1));
    EXPECT_EQ(SK_ColorCYAN, bitmap.getColor(4, 4));
  }

  {
    // Change highlight opacity.
    state_.flood_fill_progress = 0.5f;
    state_.highlighted_ratio = 0.1f;
    gfx::Canvas canvas(gfx::Size(10, 10), 1.0f, true);
    SkBitmap bitmap = canvas.GetBitmap();
    painter.Paint(&canvas, gfx::Size(5.0f, 5.0f));
    EXPECT_EQ(0x19001919u, bitmap.getColor(1, 1));
    EXPECT_EQ(SK_ColorCYAN, bitmap.getColor(4, 4));
  }
}

}  // namespace views
