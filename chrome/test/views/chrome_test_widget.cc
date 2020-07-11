// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/views/chrome_test_widget.h"

#include <memory>

#include "ui/base/theme_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace base {
class RefCountedMemory;
}

namespace gfx {
class ImageSkia;
}

class ChromeTestWidget::StubThemeProvider : public ui::ThemeProvider {
 public:
  StubThemeProvider() = default;
  ~StubThemeProvider() override = default;

  // ui::ThemeProvider:
  gfx::ImageSkia* GetImageSkiaNamed(int id) const override { return nullptr; }
  SkColor GetColor(int id) const override { return gfx::kPlaceholderColor; }
  color_utils::HSL GetTint(int id) const override { return color_utils::HSL(); }
  int GetDisplayProperty(int id) const override { return -1; }
  bool ShouldUseNativeFrame() const override { return false; }
  bool HasCustomImage(int id) const override { return false; }
  bool HasCustomColor(int id) const override { return false; }
  base::RefCountedMemory* GetRawData(int id, ui::ScaleFactor scale_factor)
      const override {
    return nullptr;
  }
};

ChromeTestWidget::ChromeTestWidget()
    : theme_provider_(std::make_unique<StubThemeProvider>()) {}

ChromeTestWidget::~ChromeTestWidget() = default;

const ui::ThemeProvider* ChromeTestWidget::GetThemeProvider() const {
  return theme_provider_.get();
}
