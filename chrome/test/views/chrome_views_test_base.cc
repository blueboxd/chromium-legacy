// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/views/chrome_views_test_base.h"

#include <memory>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

#if defined(OS_CHROMEOS)
#include "ash/test/ash_test_helper.h"
#include "ui/views/test/views_test_helper_aura.h"
#endif

namespace {

#if defined(OS_CHROMEOS)
std::unique_ptr<aura::test::AuraTestHelper> MakeTestHelper() {
  return std::make_unique<ash::AshTestHelper>();
}
#endif

class StubThemeProvider : public ui::ThemeProvider {
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

class TestWidget : public views::Widget {
 public:
  TestWidget() = default;
  ~TestWidget() override = default;

  // views::Widget:
  const ui::ThemeProvider* GetThemeProvider() const override {
    return &theme_provider_;
  }

 private:
  StubThemeProvider theme_provider_;
};

}  // namespace

ChromeViewsTestBase::ChromeViewsTestBase()
    : views::ViewsTestBase(std::unique_ptr<base::test::TaskEnvironment>(
          std::make_unique<content::BrowserTaskEnvironment>(
              content::BrowserTaskEnvironment::MainThreadType::UI,
              content::BrowserTaskEnvironment::TimeSource::MOCK_TIME))) {}

ChromeViewsTestBase::~ChromeViewsTestBase() = default;

void ChromeViewsTestBase::SetUp() {
#if defined(OS_CHROMEOS)
  views::ViewsTestHelperAura::SetAuraTestHelperFactory(&MakeTestHelper);
#endif

  views::ViewsTestBase::SetUp();

  // This is similar to calling set_test_views_delegate() with a
  // ChromeTestViewsDelegate before the superclass SetUp(); however, this allows
  // the framework to provide whatever TestViewsDelegate subclass it likes as a
  // base.
  test_views_delegate()->set_layout_provider(
      ChromeLayoutProvider::CreateLayoutProvider());
}

#if defined(OS_CHROMEOS)
void ChromeViewsTestBase::TearDown() {
  views::ViewsTestHelperAura::SetAuraTestHelperFactory(nullptr);

  views::ViewsTestBase::TearDown();
}
#endif

std::unique_ptr<views::Widget> ChromeViewsTestBase::AllocateTestWidget() {
  return std::make_unique<TestWidget>();
}
