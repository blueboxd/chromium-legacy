// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/hud_display.h"

#include "ash/fast_ink/view_tree_host_root_view.h"
#include "ash/fast_ink/view_tree_host_widget.h"
#include "ash/hud_display/graphs_container_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace hud_display {
namespace {

std::unique_ptr<views::Widget> g_hud_widget;

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

HUDDisplayView::HUDDisplayView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  SetBackground(views::CreateSolidBackground(kBackground));
  SetBorder(views::CreateEmptyBorder(gfx::Insets(5)));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  AddChildView(std::make_unique<GraphsContainerView>());
}

HUDDisplayView::~HUDDisplayView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
}

////////////////////////////////////////////////////////////////////////////////

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
