// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_CONTAINER_H_
#define ASH_SYSTEM_TRAY_TRAY_CONTAINER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "ui/views/view.h"

namespace ash {
class Shelf;

// Base class for tray containers. Sets the border and layout. The container
// auto-resizes the widget when necessary.
class TrayContainer : public views::View {
 public:
  explicit TrayContainer(Shelf* shelf);
  ~TrayContainer() override;

  void UpdateAfterShelfChange();

  void SetMargin(int main_axis_margin, int cross_axis_margin);

 protected:
  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(View* child) override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  gfx::Rect GetAnchorBoundsInScreen() const override;
  const char* GetClassName() const override;

 private:
  struct LayoutInputs {
    bool shelf_alignment_is_horizontal = true;
    int status_area_hit_region_padding = 0;
    gfx::Rect anchor_bounds_in_screen;
    int main_axis_margin = 0;
    int cross_axis_margin = 0;

    bool operator==(const LayoutInputs& other) const {
      return shelf_alignment_is_horizontal ==
                 other.shelf_alignment_is_horizontal &&
             status_area_hit_region_padding ==
                 other.status_area_hit_region_padding &&
             anchor_bounds_in_screen == other.anchor_bounds_in_screen &&
             main_axis_margin == other.main_axis_margin &&
             cross_axis_margin == other.cross_axis_margin;
    }
  };

  // Collects the inputs for layout.
  LayoutInputs GetLayoutInputs() const;

  void UpdateLayout();

  // The set of inputs that impact this widget's layout. The assumption is that
  // this widget needs a relayout if, and only if, one or more of these has
  // changed.
  base::Optional<LayoutInputs> layout_inputs_;

  Shelf* const shelf_;

  int main_axis_margin_ = 0;
  int cross_axis_margin_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TrayContainer);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_CONTAINER_H_
