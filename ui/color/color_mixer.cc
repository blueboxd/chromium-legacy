// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_mixer.h"

#include "base/containers/adapters.h"
#include "base/stl_util.h"
#include "ui/color/color_recipe.h"
#include "ui/gfx/color_palette.h"

namespace ui {

ColorMixer::ColorMixer(const ColorMixer* previous_mixer)
    : previous_mixer_(previous_mixer) {}

ColorMixer::ColorMixer(ColorMixer&&) noexcept = default;

ColorMixer& ColorMixer::operator=(ColorMixer&&) noexcept = default;

ColorMixer::~ColorMixer() = default;

void ColorMixer::AddSet(ColorSet&& set) {
  DCHECK(FindSetWithId(set.id) == sets_.cend());
  sets_.push_front(std::move(set));
}

ColorRecipe& ColorMixer::AddRecipe(ColorId id) {
  DCHECK_COLOR_ID_VALID(id);
  DCHECK(!base::Contains(recipes_, id));
  return recipes_[id];
}

SkColor ColorMixer::GetInputColor(ColorId id) const {
  DCHECK_COLOR_ID_VALID(id);
  for (const auto& set : sets_) {
    const auto i = set.colors.find(id);
    if (i != set.colors.end())
      return i->second;
  }
  return previous_mixer_ ? previous_mixer_->GetResultColor(id)
                         : gfx::kPlaceholderColor;
}

SkColor ColorMixer::GetOriginalColorFromSet(ColorId id,
                                            ColorSetId set_id) const {
  DCHECK_COLOR_ID_VALID(id);
  DCHECK_COLOR_SET_ID_VALID(set_id);
  const auto i = FindSetWithId(set_id);
  if (i != sets_.end()) {
    const auto j = i->colors.find(id);
    if (j != i->colors.end())
      return j->second;
  }
  return previous_mixer_ ? previous_mixer_->GetOriginalColorFromSet(id, set_id)
                         : gfx::kPlaceholderColor;
}

SkColor ColorMixer::GetResultColor(ColorId id) const {
  DCHECK_COLOR_ID_VALID(id);
  const SkColor color = GetInputColor(id);
  const auto i = recipes_.find(id);
  return (i == recipes_.end()) ? color : i->second.GenerateResult(color, *this);
}

ColorMixer::ColorSets::const_iterator ColorMixer::FindSetWithId(
    ColorSetId id) const {
  return std::find_if(sets_.cbegin(), sets_.cend(),
                      [id](const auto& set) { return set.id == id; });
}

}  // namespace ui
