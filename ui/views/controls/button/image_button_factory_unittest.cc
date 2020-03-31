// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/image_button_factory.h"

#include <memory>
#include <utility>

#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/test/ink_drop_host_view_test_api.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/views_test_base.h"

namespace views {

using ImageButtonFactoryTest = ViewsTestBase;

TEST_F(ImageButtonFactoryTest, CreateVectorImageButton) {
  auto button = CreateVectorImageButton(nullptr);
  EXPECT_EQ(ImageButton::ALIGN_CENTER, button->h_alignment_);
  EXPECT_EQ(ImageButton::ALIGN_MIDDLE, button->v_alignment_);
}

TEST_F(ImageButtonFactoryTest, SetImageFromVectorIcon) {
  auto button = CreateVectorImageButton(nullptr);
  SetImageFromVectorIcon(button.get(), vector_icons::kCloseRoundedIcon,
                         SK_ColorRED);
  EXPECT_FALSE(button->GetImage(Button::STATE_NORMAL).isNull());
  EXPECT_FALSE(button->GetImage(Button::STATE_DISABLED).isNull());
  EXPECT_EQ(color_utils::DeriveDefaultIconColor(SK_ColorRED),
            button->GetInkDropBaseColor());
}

class ImageButtonFactoryWidgetTest : public ViewsTestBase {
 public:
  ImageButtonFactoryWidgetTest() = default;
  ~ImageButtonFactoryWidgetTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    // Create a widget so that buttons can get access to their NativeTheme
    // instance.
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget_->Init(std::move(params));
    widget_->Show();
  }

  void TearDown() override {
    button_.reset();
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  ImageButton* AddImageButton(std::unique_ptr<ImageButton> button) {
    button_ = std::move(button);
    widget_->SetContentsView(button_.get());
    return button_.get();
  }

 protected:
  Widget* widget() { return widget_.get(); }
  ImageButton* button() { return button_.get(); }

 private:
  std::unique_ptr<Widget> widget_;
  std::unique_ptr<ImageButton> button_;

  DISALLOW_COPY_AND_ASSIGN(ImageButtonFactoryWidgetTest);
};

TEST_F(ImageButtonFactoryWidgetTest, CreateVectorImageButtonWithNativeTheme) {
  AddImageButton(CreateVectorImageButtonWithNativeTheme(
      nullptr, vector_icons::kCloseRoundedIcon));
  EXPECT_EQ(button()->GetNativeTheme()->GetSystemColor(
                ui::NativeTheme::kColorId_DefaultIconColor),
            button()->GetInkDropBaseColor());
}
}  // namespace views
