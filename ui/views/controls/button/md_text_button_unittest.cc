// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/md_text_button.h"

#include "ui/views/test/views_test_base.h"

namespace views {

using MdTextButtonTest = ViewsTestBase;

TEST_F(MdTextButtonTest, CustomPadding) {
  const base::string16 text = base::ASCIIToUTF16("abc");
  std::unique_ptr<MdTextButton> button =
      MdTextButton::Create(nullptr, text, views::style::CONTEXT_BUTTON_MD);

  const gfx::Insets custom_padding(10, 20);
  ASSERT_NE(button->GetInsets(), custom_padding);

  button->SetCustomPadding(custom_padding);
  EXPECT_EQ(button->GetInsets(), custom_padding);
}

}  // namespace views
