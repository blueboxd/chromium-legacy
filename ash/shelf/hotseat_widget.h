// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_HOTSEAT_WIDGET_H_
#define ASH_SHELF_HOTSEAT_WIDGET_H_

#include "ash/ash_export.h"
#include "ui/views/widget/widget.h"

namespace ash {
class Shelf;
class ShelfView;

// The hotseat widget is part of the shelf and hosts app shortcuts.
// TODO(crbug.com/973446): Move |ShelfView| from |ShelfWidget| to this widget.
class ASH_EXPORT HotseatWidget : public views::Widget {
 public:
  HotseatWidget(Shelf* shelf, ShelfView* shelf_view);

  // Initializes the widget and sets its basic properties.
  void Initialize(aura::Window* container);

 private:
  // View containing the shelf items within an active user session. Owned by
  // the views hierarchy.
  ShelfView* shelf_view_;

  DISALLOW_COPY_AND_ASSIGN(HotseatWidget);
};

}  // namespace ash

#endif  // ASH_SHELF_HOTSEAT_WIDGET_H_
