// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/hud_display.h"

#include "ash/fast_ink/view_tree_host_root_view.h"
#include "ash/fast_ink/view_tree_host_widget.h"
#include "ash/hud_display/graphs_container_view.h"
#include "ash/hud_display/hud_constants.h"
#include "ash/hud_display/hud_properties.h"
#include "ash/hud_display/hud_settings_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace hud_display {
namespace {

constexpr int kVectorIconSize = 18;

std::unique_ptr<views::Widget> g_hud_widget;

constexpr SkColor kBackground = SkColorSetARGB(kHUDAlpha, 17, 17, 17);

// Basically views::SolidBackground with SkBlendMode::kSrc paint mode.
class SolidSourceBackground : public views::Background {
 public:
  explicit SolidSourceBackground(SkColor color) {
    SetNativeControlColor(color);
  }

  SolidSourceBackground(const SolidSourceBackground&) = delete;
  SolidSourceBackground& operator=(const SolidSourceBackground&) = delete;

  void Paint(gfx::Canvas* canvas, views::View* /*view*/) const override {
    // Fill the background. Note that we don't constrain to the bounds as
    // canvas is already clipped for us.
    canvas->DrawColor(get_color(), SkBlendMode::kSrc);
  }
};

std::unique_ptr<views::View> CreateButtonsContainer() {
  auto container = std::make_unique<views::View>();
  auto layout_manager = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  container->SetLayoutManager(std::move(layout_manager));
  return container;
}

std::unique_ptr<views::ImageButton> CreateSettingsButton(HUDDisplayView* hud) {
  auto button = std::make_unique<views::ImageButton>(hud);
  button->SetVisible(false);
  button->SetImage(
      views::Button::ButtonState::STATE_NORMAL,
      gfx::CreateVectorIcon(vector_icons::kSettingsIcon, kVectorIconSize,
                            HUDSettingsView::kDefaultColor));
  button->SetBorder(views::CreateEmptyBorder(gfx::Insets(5)));
  button->SetProperty(kHUDClickHandler, HTCLIENT);
  button->SetBackground(std::make_unique<SolidSourceBackground>(kBackground));
  return button;
}

class HUDOverlayContainerView : public views::View {
 public:
  METADATA_HEADER(HUDOverlayContainerView);

  explicit HUDOverlayContainerView(HUDDisplayView* hud);
  ~HUDOverlayContainerView() override = default;

  HUDOverlayContainerView(const HUDOverlayContainerView&) = delete;
  HUDOverlayContainerView& operator=(const HUDOverlayContainerView&) = delete;

  HUDSettingsView* settings_view() const { return settings_view_; }
  views::ImageButton* settings_trigger_button() const {
    return settings_trigger_button_;
  }

 private:
  HUDSettingsView* settings_view_ = nullptr;
  views::ImageButton* settings_trigger_button_ = nullptr;
};

BEGIN_METADATA(HUDOverlayContainerView)
METADATA_PARENT_CLASS(View)
END_METADATA()

HUDOverlayContainerView::HUDOverlayContainerView(HUDDisplayView* hud) {
  // Overlay container has two child views stacked vertically and stretched
  // horizontally.
  // The top is a container for "Settings" button.
  // The bottom is Settings UI view.
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  {
    // Buttons container arranges buttons horizontally and does not alter
    // button sizes.
    views::View* buttons_container = AddChildView(CreateButtonsContainer());
    settings_trigger_button_ =
        buttons_container->AddChildView(CreateSettingsButton(hud));
  }
  // HUDSettingsView starts invisible.
  settings_view_ = AddChildView(std::make_unique<HUDSettingsView>());

  // Make settings view occupy all the remaining space.
  layout_manager->SetFlexForView(settings_view_, 1,
                                 /*use_min_size=*/false);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// HUDDisplayView, public:

BEGIN_METADATA(HUDDisplayView)
METADATA_PARENT_CLASS(WidgetDelegateView)
END_METADATA()

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
  widget->GetLayer()->SetName("HUDDisplayView");
  widget->Show();

  g_hud_widget = base::WrapUnique(widget);
}

HUDDisplayView::HUDDisplayView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  SetBackground(views::CreateSolidBackground(kBackground));
  SetBorder(views::CreateEmptyBorder(gfx::Insets(5)));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  // We have two child views z-stacked.
  // The bottom one is GraphsContainerView with all the graph lines.
  // The top one lays out buttons and settings UI overlays.
  graphs_container_ = AddChildView(std::make_unique<GraphsContainerView>());

  HUDOverlayContainerView* overlay_container =
      AddChildView(std::make_unique<HUDOverlayContainerView>(this));
  settings_view_ = overlay_container->settings_view();
  settings_trigger_button_ = overlay_container->settings_trigger_button();

  // Receive OnMouseEnter/OnMouseLEave when hovering over the child views too.
  set_notify_enter_exit_on_child(true);
}

HUDDisplayView::~HUDDisplayView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
}

////////////////////////////////////////////////////////////////////////////////

void HUDDisplayView::OnMouseEntered(const ui::MouseEvent& /*event*/) {
  settings_trigger_button_->SetVisible(true);
}

void HUDDisplayView::OnMouseExited(const ui::MouseEvent& /*event*/) {
  // Button is always visible if Settings UI is visible.
  if (settings_view_->GetVisible())
    return;

  settings_trigger_button_->SetVisible(false);
}

int HUDDisplayView::NonClientHitTest(const gfx::Point& point) {
  const View* view = GetEventHandlerForPoint(point);
  if (!view)
    return HTNOWHERE;

  return view->GetProperty(kHUDClickHandler);
}

// ClientView that return HTNOWHERE by default. A child view can receive event
// by setting kHitTestComponentKey property to HTCLIENT.
class HTClientView : public views::ClientView {
 public:
  HTClientView(HUDDisplayView* hud_display,
               views::Widget* widget,
               views::View* contents_view)
      : views::ClientView(widget, contents_view), hud_display_(hud_display) {}
  ~HTClientView() override = default;

  int NonClientHitTest(const gfx::Point& point) override;

 private:
  HUDDisplayView* hud_display_ = nullptr;
};

int HTClientView::NonClientHitTest(const gfx::Point& point) {
  return hud_display_->NonClientHitTest(point);
}

views::ClientView* HUDDisplayView::CreateClientView(views::Widget* widget) {
  return new HTClientView(this, widget, GetContentsView());
}

void HUDDisplayView::OnWidgetInitialized() {
  auto* frame_view = GetWidget()->non_client_view()->frame_view();
  // TODO(oshima): support component type with TYPE_WINDOW_FLAMELESS widget.
  if (frame_view) {
    frame_view->SetEnabled(false);
    frame_view->SetVisible(false);
  }
}

// There is only one button.
void HUDDisplayView::ButtonPressed(views::Button* /*sender*/,
                                   const ui::Event& /*event*/) {
  settings_view_->ToggleVisibility();
  graphs_container_->SetVisible(!settings_view_->GetVisible());
}

}  // namespace hud_display
}  // namespace ash
