// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/themed_vector_icon.h"

#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

namespace ui {

ThemedVectorIcon::ThemedVectorIcon() = default;

ThemedVectorIcon::ThemedVectorIcon(const gfx::VectorIcon* icon,
                                   NativeTheme::ColorId color_id)
    : icon_(icon), color_id_(color_id) {}

ThemedVectorIcon::ThemedVectorIcon(const gfx::VectorIcon* icon, SkColor color)
    : icon_(icon), color_(color) {}

ThemedVectorIcon::ThemedVectorIcon(const ThemedVectorIcon& other) = default;

const gfx::ImageSkia ThemedVectorIcon::GetImageSkia(NativeTheme* theme) const {
  DCHECK(!empty());
  return CreateVectorIcon(*icon_, GetColor(theme));
}

const gfx::ImageSkia ThemedVectorIcon::GetImageSkia(NativeTheme* theme,
                                                    int icon_size) const {
  DCHECK(!empty());
  return CreateVectorIcon(*icon_, icon_size, GetColor(theme));
}

const gfx::ImageSkia ThemedVectorIcon::GetImageSkia(SkColor color) const {
  DCHECK(!empty());
  return CreateVectorIcon(*icon_, color);
}

SkColor ThemedVectorIcon::GetColor(NativeTheme* theme) const {
  DCHECK(color_id_ || color_);
  return color_id_ ? theme->GetSystemColor(color_id_.value()) : color_.value();
}

}  // namespace ui