// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_DRAG_HANDLE_H_
#define ASH_SHELF_DRAG_HANDLE_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/style/ash_color_provider.h"
#include "base/timer/timer.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"

namespace ash {

class ASH_EXPORT DragHandle : public views::View,
                              public views::ViewTargeterDelegate {
 public:
  explicit DragHandle(gfx::Size drag_handle_size,
                      AshColorProvider::RippleAttributes ripple_attributes,
                      int drag_handle_corner_radius);
  DragHandle(const DragHandle&) = delete;
  ~DragHandle() override;

  DragHandle& operator=(const DragHandle&) = delete;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;
};

}  // namespace ash

#endif  // ASH_SHELF_DRAG_HANDLE_H_
