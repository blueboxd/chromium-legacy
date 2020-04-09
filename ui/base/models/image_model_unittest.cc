// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/image_model.h"

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

namespace ui {

namespace {

const gfx::VectorIcon& GetVectorIcon() {
  static constexpr gfx::PathElement path[] = {gfx::CommandType::CIRCLE, 24, 18,
                                              5};
  static const gfx::VectorIconRep rep[] = {{path, 4}};
  static constexpr gfx::VectorIcon circle_icon = {rep, 1, "circle"};

  return circle_icon;
}

}  // namespace

TEST(ImageModelTest, DefaultEmpty) {
  ImageModel image_model;

  EXPECT_TRUE(image_model.IsEmpty());
}

TEST(ImageModelTest, DefaultVectorIconEmpty) {
  VectorIconModel vector_icon_model;

  EXPECT_TRUE(vector_icon_model.is_empty());
}

TEST(ImageModelTest, CheckForVectorIcon) {
  ImageModel image_model = ImageModel::FromVectorIcon(GetVectorIcon());

  EXPECT_FALSE(image_model.IsEmpty());
  EXPECT_TRUE(image_model.IsVectorIcon());
}

TEST(ImageModelTest, CheckForImage) {
  ImageModel image_model =
      ImageModel::FromImage(gfx::test::CreateImage(16, 16));

  EXPECT_FALSE(image_model.IsEmpty());
  EXPECT_TRUE(image_model.IsImage());
}

TEST(ImageModelTest, CheckAssignVectorIcon) {
  VectorIconModel vector_icon_model_dest;
  VectorIconModel vector_icon_model_src =
      ImageModel::FromVectorIcon(GetVectorIcon()).GetVectorIcon();

  EXPECT_TRUE(vector_icon_model_dest.is_empty());
  EXPECT_FALSE(vector_icon_model_src.is_empty());

  vector_icon_model_dest = vector_icon_model_src;
  EXPECT_FALSE(vector_icon_model_dest.is_empty());
}

TEST(ImageModelTest, CheckAssignImage) {
  ImageModel image_model_dest;
  ImageModel image_model_src =
      ImageModel::FromImage(gfx::test::CreateImage(16, 16));

  EXPECT_TRUE(image_model_dest.IsEmpty());
  EXPECT_FALSE(image_model_src.IsEmpty());
  EXPECT_TRUE(image_model_src.IsImage());
  EXPECT_FALSE(image_model_src.IsVectorIcon());

  image_model_dest = image_model_src;

  EXPECT_FALSE(image_model_dest.IsEmpty());
  EXPECT_TRUE(image_model_dest.IsImage());
  EXPECT_FALSE(image_model_dest.IsVectorIcon());

  image_model_src = ImageModel::FromVectorIcon(GetVectorIcon());

  EXPECT_TRUE(image_model_src.IsVectorIcon());

  image_model_dest = image_model_src;

  EXPECT_TRUE(image_model_dest.IsVectorIcon());
  EXPECT_FALSE(image_model_dest.IsImage());
}

}  // namespace ui
