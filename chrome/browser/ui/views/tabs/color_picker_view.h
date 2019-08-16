// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COLOR_PICKER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COLOR_PICKER_VIEW_H_

#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/optional.h"
#include "ui/views/view.h"

namespace views {
class Button;
}

class ColorPickerElementView;

// Lets users pick from a list of colors displayed as circles that can be
// clicked on. At most one can be selected, similar to radio buttons.
//
// TODO(crbug.com/989174): make this keyboard and screenreader accessible.
class ColorPickerView : public views::View {
 public:
  // |colors| should contain the color values and accessible names. There should
  // not be duplicate colors.
  explicit ColorPickerView(
      base::span<const std::pair<SkColor, base::string16>> colors);
  ~ColorPickerView() override;

  base::Optional<SkColor> GetSelectedColor() const;

  // views::View:
  views::View* GetSelectedViewForGroup(int group) override;

  views::Button* GetElementAtIndexForTesting(int index);

 private:
  // Handles the selection of a particular color. This is passed as a callback
  // to the views representing each color.
  void OnColorSelected(ColorPickerElementView* element);

  std::vector<ColorPickerElementView*> elements_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COLOR_PICKER_VIEW_H_
