// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/hud_display.h"

#include <unistd.h>
#include <cmath>
#include <numeric>

#include "ash/fast_ink/view_tree_host_root_view.h"
#include "ash/fast_ink/view_tree_host_widget.h"
#include "ash/hud_display/data_source.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/task/post_task.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace hud_display {
namespace {

std::unique_ptr<views::Widget> g_hud_widget;

// UI refresh interval.
constexpr base::TimeDelta kHUDRefreshInterval =
    base::TimeDelta::FromMilliseconds(500);

constexpr SkColor kBackground = SkColorSetARGB(208, 17, 17, 17);

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// HUDDisplayView, public:

// static
void HUDDisplayView::Destroy() {
  g_hud_widget.reset();
}

void HUDDisplayView::Toggle() {
  if (g_hud_widget) {
    Destroy();
    return;
  }

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = new HUDDisplayView();
  params.parent = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                      kShellWindowId_OverlayContainer);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds =
      gfx::Rect(Graph::kDefaultWidth + 2 * kHUDInset, kDefaultHUDHeight);
  auto* widget = CreateViewTreeHostWidget(std::move(params));
  widget->Show();

  g_hud_widget = base::WrapUnique(widget);
}

HUDDisplayView::HUDDisplayView()
    : graph_chrome_rss_private_(Graph::Baseline::BASELINE_BOTTOM,
                                Graph::Fill::SOLID,
                                SK_ColorRED),
      graph_mem_free_(Graph::Baseline::BASELINE_BOTTOM,
                      Graph::Fill::NONE,
                      SK_ColorDKGRAY),
      graph_mem_used_unknown_(Graph::Baseline::BASELINE_BOTTOM,
                              Graph::Fill::SOLID,
                              SK_ColorLTGRAY),
      graph_renderers_rss_private_(Graph::Baseline::BASELINE_BOTTOM,
                                   Graph::Fill::SOLID,
                                   SK_ColorCYAN),
      graph_arc_rss_private_(Graph::Baseline::BASELINE_BOTTOM,
                             Graph::Fill::SOLID,
                             SK_ColorMAGENTA),
      graph_gpu_rss_private_(Graph::Baseline::BASELINE_BOTTOM,
                             Graph::Fill::SOLID,
                             SK_ColorRED),
      graph_gpu_kernel_(Graph::Baseline::BASELINE_BOTTOM,
                        Graph::Fill::SOLID,
                        SK_ColorYELLOW),
      graph_chrome_rss_shared_(Graph::Baseline::BASELINE_BOTTOM,
                               Graph::Fill::NONE,
                               SK_ColorBLUE) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  refresh_timer_.Start(FROM_HERE, kHUDRefreshInterval,
                       static_cast<views::View*>(this),
                       &views::View::SchedulePaint);
  SetBackground(views::CreateSolidBackground(kBackground));
}

HUDDisplayView::~HUDDisplayView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
}

////////////////////////////////////////////////////////////////////////////////

void HUDDisplayView::OnPaint(gfx::Canvas* canvas) {
  OnPaintBackground(canvas);
  // TODO: Should probably update last graph point more often than shift graph.
  const DataSource::Snapshot snapshot = data_source_.GetSnapshotAndReset();

  const double total = snapshot.total_ram;
  // Nothing to do if data is not available yet.
  if (total < 1)
    return;

  const float chrome_rss_private =
      (snapshot.browser_rss - snapshot.browser_rss_shared) / total;
  const float mem_free = snapshot.free_ram / total;
  // mem_used_unknown is calculated below.
  const float renderers_rss_private =
      (snapshot.renderers_rss - snapshot.renderers_rss_shared) / total;
  const float arc_rss_private =
      (snapshot.arc_rss - snapshot.arc_rss_shared) / total;
  const float gpu_rss_private =
      (snapshot.gpu_rss - snapshot.gpu_rss_shared) / total;
  const float gpu_kernel = snapshot.gpu_kernel / total;

  // not stacked.
  const float chrome_rss_shared = snapshot.browser_rss_shared / total;

  std::vector<float> used_buckets;
  used_buckets.push_back(chrome_rss_private);
  used_buckets.push_back(mem_free);
  used_buckets.push_back(renderers_rss_private);
  used_buckets.push_back(arc_rss_private);
  used_buckets.push_back(gpu_rss_private);
  used_buckets.push_back(gpu_kernel);

  float mem_used_unknown =
      1 - std::accumulate(used_buckets.begin(), used_buckets.end(), 0.0f);

  if (mem_used_unknown < 0)
    LOG(WARNING) << "mem_used_unknown=" << mem_used_unknown << " < 0 !";

  // Update graph data.
  graph_chrome_rss_private_.AddValue(chrome_rss_private);
  graph_mem_free_.AddValue(mem_free);
  graph_mem_used_unknown_.AddValue(std::max(mem_used_unknown, 0.0f));
  graph_renderers_rss_private_.AddValue(renderers_rss_private);
  graph_arc_rss_private_.AddValue(arc_rss_private);
  graph_gpu_rss_private_.AddValue(gpu_rss_private);
  graph_gpu_kernel_.AddValue(gpu_kernel);
  // Not stacked.
  graph_chrome_rss_shared_.AddValue(chrome_rss_shared);

  gfx::Rect memory_stats_rect = GetLocalBounds();
  memory_stats_rect.Inset(kHUDInset, kHUDInset);
  // Layout graphs.
  graph_chrome_rss_private_.Layout(memory_stats_rect, nullptr /* base*/);
  graph_mem_free_.Layout(memory_stats_rect, &graph_chrome_rss_private_);
  graph_mem_used_unknown_.Layout(memory_stats_rect, &graph_mem_free_);
  graph_renderers_rss_private_.Layout(memory_stats_rect,
                                      &graph_mem_used_unknown_);
  graph_arc_rss_private_.Layout(memory_stats_rect,
                                &graph_renderers_rss_private_);
  graph_gpu_rss_private_.Layout(memory_stats_rect, &graph_arc_rss_private_);
  graph_gpu_kernel_.Layout(memory_stats_rect, &graph_gpu_rss_private_);
  // Not stacked.
  graph_chrome_rss_shared_.Layout(memory_stats_rect, nullptr /* base*/);

  // Paint damaged area now that all parameters have been determined.
  {
    graph_chrome_rss_private_.Draw(canvas);
    graph_mem_free_.Draw(canvas);
    graph_mem_used_unknown_.Draw(canvas);
    graph_renderers_rss_private_.Draw(canvas);
    graph_arc_rss_private_.Draw(canvas);
    graph_gpu_rss_private_.Draw(canvas);
    graph_gpu_kernel_.Draw(canvas);

    graph_chrome_rss_shared_.Draw(canvas);
  }
}

// ClientView that return HTNOWHERE by default. A child view can receive event
// by setting kHitTestComponentKey property to HTCLIENT.
class HTClientView : public views::ClientView {
 public:
  HTClientView(views::Widget* widget, views::View* contents_view)
      : views::ClientView(widget, contents_view) {}
  ~HTClientView() override = default;

  int NonClientHitTest(const gfx::Point& point) override { return HTNOWHERE; }
};

views::ClientView* HUDDisplayView::CreateClientView(views::Widget* widget) {
  return new HTClientView(widget, GetContentsView());
}

void HUDDisplayView::OnWidgetInitialized() {
  auto* frame_view = GetWidget()->non_client_view()->frame_view();
  // TODO(oshima): support component type with TYPE_WINDOW_FLAMELESS widget.
  if (frame_view) {
    frame_view->SetEnabled(false);
    frame_view->SetVisible(false);
  }
}

}  // namespace hud_display
}  // namespace ash
