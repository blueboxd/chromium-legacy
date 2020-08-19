// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/cpu_graph_page_view.h"

#include <numeric>

#include "ash/hud_display/hud_constants.h"
#include "ui/gfx/canvas.h"

namespace ash {
namespace hud_display {

////////////////////////////////////////////////////////////////////////////////
// CpuGraphPageView, public:

BEGIN_METADATA(CpuGraphPageView)
METADATA_PARENT_CLASS(GraphPageViewBase)
END_METADATA()

CpuGraphPageView::CpuGraphPageView()
    : cpu_other_(Graph::Baseline::BASELINE_BOTTOM,
                 Graph::Fill::SOLID,
                 SkColorSetA(SK_ColorMAGENTA, kHUDAlpha)),
      cpu_system_(Graph::Baseline::BASELINE_BOTTOM,
                  Graph::Fill::SOLID,
                  SkColorSetA(SK_ColorRED, kHUDAlpha)),
      cpu_user_(Graph::Baseline::BASELINE_BOTTOM,
                Graph::Fill::SOLID,
                SkColorSetA(SK_ColorBLUE, kHUDAlpha)),
      cpu_idle_(Graph::Baseline::BASELINE_BOTTOM,
                Graph::Fill::SOLID,
                SkColorSetA(SK_ColorDKGRAY, kHUDAlpha)) {}

CpuGraphPageView::~CpuGraphPageView() = default;

////////////////////////////////////////////////////////////////////////////////

void CpuGraphPageView::OnPaint(gfx::Canvas* canvas) {
  // TODO: Should probably update last graph point more often than shift graph.

  // Layout graphs.
  const gfx::Rect rect = GetContentsBounds();
  cpu_other_.Layout(rect, nullptr /* base*/);
  cpu_system_.Layout(rect, &cpu_other_);
  cpu_user_.Layout(rect, &cpu_system_);
  cpu_idle_.Layout(rect, &cpu_user_);

  // Paint damaged area now that all parameters have been determined.
  cpu_other_.Draw(canvas);
  cpu_system_.Draw(canvas);
  cpu_user_.Draw(canvas);
  cpu_idle_.Draw(canvas);
}

void CpuGraphPageView::UpdateData(const DataSource::Snapshot& snapshot) {
  // TODO: Should probably update last graph point more often than shift graph.
  const float total = snapshot.cpu_idle_part + snapshot.cpu_user_part +
                      snapshot.cpu_system_part + snapshot.cpu_other_part;
  // Nothing to do if data is not available yet (sum < 1%).
  if (total < 0.01)
    return;

  // Assume total already equals 1, no need to re-weight.

  // Update graph data.
  cpu_other_.AddValue(snapshot.cpu_other_part);
  cpu_system_.AddValue(snapshot.cpu_system_part);
  cpu_user_.AddValue(snapshot.cpu_user_part);
  cpu_idle_.AddValue(snapshot.cpu_idle_part);
}

}  // namespace hud_display
}  // namespace ash
