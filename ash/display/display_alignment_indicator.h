// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_ALIGNMENT_INDICATOR_H_
#define ASH_DISPLAY_DISPLAY_ALIGNMENT_INDICATOR_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace display {
class Display;
}  // namespace display

namespace ash {

class IndicatorHighlightView;
class IndicatorPillView;

// DisplayAlignmentIndicator is a container for indicator highlighting a shared
// edge between two displays and a pill that contains an arrow and target
// display's name.
class ASH_EXPORT DisplayAlignmentIndicator {
 public:
  // Construct and show indicator highlight and pill.
  // |src_display| is the display that the indicator is shown in.
  // |bounds| is the position and size of the 1px thick shared edge between
  // |src_display| and target display specified by |target_name|. |target_name|
  // is the target display's name that is shown in the display settings. Pill
  // does not render if |target_name| is an empty string.
  DisplayAlignmentIndicator(const display::Display& src_display,
                            const gfx::Rect& bounds,
                            const std::string& target_name);
  DisplayAlignmentIndicator(const DisplayAlignmentIndicator&) = delete;
  DisplayAlignmentIndicator& operator=(const DisplayAlignmentIndicator&) =
      delete;
  ~DisplayAlignmentIndicator();

  // Shows/Hides the indicator.
  void Show();
  void Hide();

 private:
  friend class DisplayAlignmentIndicatorTest;

  // View and Widget for showing the blue indicator highlights on the edge of
  // the display.
  IndicatorHighlightView* indicator_view_ = nullptr;  // NOT OWNED
  views::Widget indicator_widget_;

  // View and Widget for showing a pill with name of the neighboring display and
  // an arrow pointing towards it. May not be initialized if ctor without
  // |target_name| is used (for preview indicator).
  IndicatorPillView* pill_view_ = nullptr;  // NOT OWNED
  std::unique_ptr<views::Widget> pill_widget_;
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_ALIGNMENT_INDICATOR_H_
