// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_GRAPHS_CONTAINER_VIEW_H_
#define ASH_HUD_DISPLAY_GRAPHS_CONTAINER_VIEW_H_

#include "ash/hud_display/data_source.h"
#include "ash/hud_display/graph_page_view_base.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "ui/views/view.h"

namespace ash {
namespace hud_display {

enum class DisplayMode;

// GraphsContainerView class draws a bunch of graphs.
class GraphsContainerView : public views::View {
 public:
  METADATA_HEADER(GraphsContainerView);

  GraphsContainerView();
  GraphsContainerView(const GraphsContainerView&) = delete;
  GraphsContainerView& operator=(const GraphsContainerView&) = delete;
  ~GraphsContainerView() override;

  // Synchrnously reads system counters and updates data.
  void UpdateData();

  // Updates graphs display to match given mode.
  void SetMode(DisplayMode mode);

 private:
  // HUD is updatd with new data every tick.
  base::RepeatingTimer refresh_timer_;

  // Source of graphs data.
  DataSource data_source_;

  SEQUENCE_CHECKER(ui_sequence_checker_);
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_GRAPHS_CONTAINER_VIEW_H_
