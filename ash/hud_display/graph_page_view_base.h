// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_GRAPH_PAGE_VIEW_BASE_H_
#define ASH_HUD_DISPLAY_GRAPH_PAGE_VIEW_BASE_H_

#include "ash/hud_display/data_source.h"
#include "base/sequence_checker.h"
#include "ui/views/view.h"

namespace ash {
namespace hud_display {

// Interface for a single graph page.
class GraphPageViewBase : public views::View {
 public:
  METADATA_HEADER(GraphPageViewBase);

  GraphPageViewBase();
  GraphPageViewBase(const GraphPageViewBase&) = delete;
  GraphPageViewBase& operator=(const GraphPageViewBase&) = delete;
  ~GraphPageViewBase() override;

  // Update page data from the new snapshot.
  virtual void UpdateData(const DataSource::Snapshot& snapshot) = 0;

 private:
  SEQUENCE_CHECKER(ui_sequence_checker_);
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_GRAPH_PAGE_VIEW_BASE_H_
