// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_mini_view.h"

#include <memory>
#include <utility>

#include "ash/wm/overview/rounded_rect_view.h"
#include "ash/wm/window_preview_view.h"
#include "ash/wm/wm_highlight_item_border.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

// Foreground label color.
constexpr SkColor kLabelColor = SK_ColorWHITE;

// Horizontal padding for the label, on both sides.
constexpr int kHorizontalLabelPaddingDp = 12;

// The size in dp of the window icon shown on the overview window next to the
// title.
constexpr gfx::Size kIconSize{24, 24};

// The font delta of the window title.
constexpr int kLabelFontDelta = 2;

// Values of the backdrop.
constexpr int kBackdropBorderRoundingDp = 4;
constexpr SkColor kBackdropColor = SkColorSetA(SK_ColorWHITE, 0x24);

// Adds |child| as a child of |parent| and set it as paint to layer.
void AddChildWithLayer(views::View* parent, views::View* child) {
  parent->AddChildView(child);
  child->SetPaintToLayer();
  child->layer()->SetFillsBoundsOpaquely(false);
}

}  // namespace

WindowMiniView::~WindowMiniView() = default;

void WindowMiniView::SetBackdropVisibility(bool visible) {
  if (!backdrop_view_ && !visible)
    return;

  if (!backdrop_view_) {
    backdrop_view_ =
        new RoundedRectView(kBackdropBorderRoundingDp, kBackdropColor);
    backdrop_view_->set_can_process_events_within_subtree(false);
    AddChildWithLayer(this, backdrop_view_);
    Layout();
  }
  backdrop_view_->SetVisible(visible);
}

void WindowMiniView::SetShowPreview(bool show) {
  if (show == !!preview_view_)
    return;

  if (!show) {
    RemoveChildView(preview_view_);
    delete preview_view_;
    preview_view_ = nullptr;
    return;
  }

  if (!source_window_)
    return;

  preview_view_ = new WindowPreviewView(source_window_,
                                        /*trilinear_filtering_on_init=*/false);
  AddChildWithLayer(this, preview_view_);
  Layout();
}

void WindowMiniView::UpdatePreviewRoundedCorners(bool show) {
  if (!preview_view())
    return;

  ui::Layer* layer = preview_view()->layer();
  DCHECK(layer);
  const float scale = layer->transform().Scale2d().x();
  const float rounding =
      views::LayoutProvider::Get()->GetCornerRadiusMetric(views::EMPHASIS_LOW);
  const gfx::RoundedCornersF radii(show ? rounding / scale : 0.0f);
  layer->SetRoundedCornerRadius(radii);
  layer->SetIsFastRoundedCorner(true);
}

void WindowMiniView::UpdateBorderState(bool show) {
  border_ptr_->set_color(show ? gfx::kGoogleBlue300 : SK_ColorTRANSPARENT);
  SchedulePaint();
}

int WindowMiniView::GetMargin() const {
  return 0;
}

gfx::Rect WindowMiniView::GetHeaderBounds() const {
  gfx::Rect header_bounds = GetContentsBounds();
  header_bounds.set_height(kHeaderHeightDp);
  return header_bounds;
}

gfx::Size WindowMiniView::GetPreviewViewSize() const {
  DCHECK(preview_view_);
  return preview_view_->GetPreferredSize();
}

WindowMiniView::WindowMiniView(aura::Window* source_window)
    : source_window_(source_window) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  window_observer_.Add(source_window);

  header_view_ = new views::View();
  views::BoxLayout* layout =
      header_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kHorizontalLabelPaddingDp));
  AddChildWithLayer(this, header_view_);

  UpdateIconView();

  title_label_ =
      new views::Label(wm::GetTransientRoot(source_window_)->GetTitle());
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetAutoColorReadabilityEnabled(false);
  title_label_->SetEnabledColor(kLabelColor);
  title_label_->SetSubpixelRenderingEnabled(false);
  title_label_->SetFontList(gfx::FontList().Derive(
      kLabelFontDelta, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  AddChildWithLayer(header_view_, title_label_);
  layout->SetFlexForView(title_label_, 1);

  auto border =
      std::make_unique<WmHighlightItemBorder>(kBackdropBorderRoundingDp);
  border_ptr_ = border.get();
  SetBorder(std::move(border));
}

gfx::Rect WindowMiniView::GetContentAreaBounds() const {
  const int margin = GetMargin();
  gfx::Rect bounds(GetLocalBounds());
  bounds.Inset(margin, margin);
  bounds.Inset(0, kHeaderHeightDp, 0, 0);
  return bounds;
}

void WindowMiniView::Layout() {
  const gfx::Rect content_area_bounds = GetContentAreaBounds();
  if (backdrop_view_)
    backdrop_view_->SetBoundsRect(content_area_bounds);

  if (preview_view_) {
    gfx::Rect preview_bounds = content_area_bounds;
    preview_bounds.ClampToCenteredSize(GetPreviewViewSize());
    preview_view_->SetBoundsRect(preview_bounds);
  }

  header_view_->SetBoundsRect(GetHeaderBounds());
}

void WindowMiniView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kWindow;
  node_data->SetName(title_label_->GetText());
}

void WindowMiniView::OnWindowPropertyChanged(aura::Window* window,
                                             const void* key,
                                             intptr_t old) {
  // Update the icon if it changes in the middle of an overview or alt tab
  // session (due to device scale factor change or other).
  if (key != aura::client::kAppIconKey && key != aura::client::kWindowIconKey)
    return;

  UpdateIconView();
}

void WindowMiniView::OnWindowDestroying(aura::Window* window) {
  if (window != source_window_)
    return;

  window_observer_.RemoveAll();
  source_window_ = nullptr;
  SetShowPreview(false);
}

void WindowMiniView::OnWindowTitleChanged(aura::Window* window) {
  title_label_->SetText(wm::GetTransientRoot(window)->GetTitle());
}

void WindowMiniView::UpdateIconView() {
  aura::Window* transient_root = wm::GetTransientRoot(source_window_);
  // Prefer kAppIconKey over kWindowIconKey as the app icon is typically larger.
  gfx::ImageSkia* icon = transient_root->GetProperty(aura::client::kAppIconKey);
  if (!icon || icon->size().IsEmpty())
    icon = transient_root->GetProperty(aura::client::kWindowIconKey);
  if (!icon)
    return;

  if (!icon_view_) {
    icon_view_ = new views::ImageView();
    icon_view_->SetSize(kIconSize);
    AddChildWithLayer(header_view_, icon_view_);
  }

  icon_view_->SetImage(gfx::ImageSkiaOperations::CreateResizedImage(
      *icon, skia::ImageOperations::RESIZE_BEST, kIconSize));
}

}  // namespace ash
