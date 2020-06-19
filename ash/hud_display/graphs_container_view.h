// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_GRAPHS_CONTAINER_VIEW_H_
#define ASH_HUD_DISPLAY_GRAPHS_CONTAINER_VIEW_H_

#include "ash/hud_display/data_source.h"
#include "ash/hud_display/graph.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "ui/views/view.h"

namespace ash {
namespace hud_display {

// GraphsContainerView class draws a bunch of graphs.
class GraphsContainerView : public views::View {
 public:
  METADATA_HEADER(GraphsContainerView);

  GraphsContainerView();
  ~GraphsContainerView() override;

  GraphsContainerView(const GraphsContainerView&) = delete;
  GraphsContainerView& operator=(const GraphsContainerView&) = delete;

  // view::
  void OnPaint(gfx::Canvas* canvas) override;

  // Synchrnously reads system counters and updates data.
  void UpdateData();

 private:
  // HUD is updatd with new data every tick.
  base::RepeatingTimer refresh_timer_;

  // --- Stacked:
  // Share of the total RAM occupied by Chrome browser private RSS.
  Graph graph_chrome_rss_private_;
  // Share of the total RAM reported as Free memory be kernel.
  Graph graph_mem_free_;
  // Total RAM - other graphs in this stack.
  Graph graph_mem_used_unknown_;
  // Share of the total RAM occupied by Chrome type=renderer processes private
  // RSS.
  Graph graph_renderers_rss_private_;
  // Share of the total RAM occupied by ARC++ processes private RSS.
  Graph graph_arc_rss_private_;
  // Share of the total RAM occupied by Chrome type=gpu process private RSS.
  Graph graph_gpu_rss_private_;
  // Share of the total RAM used by kernel GPU driver.
  Graph graph_gpu_kernel_;

  // Not stacked:
  // Share of the total RAM occupied by Chrome browser process shared RSS.
  Graph graph_chrome_rss_shared_;

  DataSource data_source_;

  SEQUENCE_CHECKER(ui_sequence_checker_);
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_GRAPHS_CONTAINER_VIEW_H_
