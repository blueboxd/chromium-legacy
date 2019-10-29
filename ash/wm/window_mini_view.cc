// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_mini_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/rounded_rect_view.h"
#include "ash/wm/window_preview_view.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

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
constexpr int kBackdropRoundingDp = 4;
constexpr SkColor kBackdropColor = SkColorSetA(SK_ColorWHITE, 0x24);

void AddChildWithLayer(views::View* parent, views::View* child) {
  child->SetPaintToLayer();
  child->layer()->SetFillsBoundsOpaquely(false);
  parent->AddChildView(child);
}

}  // namespace

WindowMiniView::~WindowMiniView() = default;

void WindowMiniView::SetBackdropVisibility(bool visible) {
  if (!backdrop_view_ && !visible)
    return;

  if (!backdrop_view_) {
    backdrop_view_ = new RoundedRectView(kBackdropRoundingDp, kBackdropColor);
    backdrop_view_->set_can_process_events_within_subtree(false);
    AddChildWithLayer(this, backdrop_view_);
    Layout();
  }
  backdrop_view_->SetVisible(visible);
}

void WindowMiniView::SetTitle(const base::string16& title) {
  title_label_->SetText(title);
  SetAccessibleName(title);
}

void WindowMiniView::SetShowPreview(bool show) {
  if (show == !!preview_view_)
    return;

  if (!show) {
    RemoveChildView(preview_view_);
    preview_view_ = nullptr;
    return;
  }

  preview_view_ = new WindowPreviewView(source_window_,
                                        /*trilinear_filtering_on_init=*/false);
  AddChildWithLayer(this, preview_view_);
  Layout();
}

void WindowMiniView::UpdatePreviewRoundedCorners(bool show, float rounding) {
  if (!preview_view_)
    return;

  const float scale = preview_view_->layer()->transform().Scale2d().x();
  const gfx::RoundedCornersF radii(show ? rounding / scale : 0.0f);
  preview_view_->layer()->SetRoundedCornerRadius(radii);
  preview_view_->layer()->SetIsFastRoundedCorner(true);
}

WindowMiniView::WindowMiniView(aura::Window* source_window)
    : views::Button(nullptr), source_window_(source_window) {
  SetAccessibleName(source_window->GetTitle());

  header_view_ = new views::View();
  views::BoxLayout* layout =
      header_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kHorizontalLabelPaddingDp));
  AddChildWithLayer(this, header_view_);

  // Prefer kAppIconKey over kWindowIconKey as the app icon is typically larger.
  gfx::ImageSkia* icon = source_window->GetProperty(aura::client::kAppIconKey);
  if (!icon || icon->size().IsEmpty())
    icon = source_window->GetProperty(aura::client::kWindowIconKey);
  if (icon && !icon->size().IsEmpty()) {
    image_view_ = new views::ImageView();
    image_view_->SetImage(gfx::ImageSkiaOperations::CreateResizedImage(
        *icon, skia::ImageOperations::RESIZE_BEST, kIconSize));
    image_view_->SetSize(kIconSize);
    header_view_->AddChildView(image_view_);
  }

  title_label_ = new views::Label(source_window->GetTitle());
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetAutoColorReadabilityEnabled(false);
  title_label_->SetEnabledColor(kLabelColor);
  title_label_->SetSubpixelRenderingEnabled(false);
  title_label_->SetFontList(gfx::FontList().Derive(
      kLabelFontDelta, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  header_view_->AddChildView(title_label_);
  layout->SetFlexForView(title_label_, 1);
}

void WindowMiniView::Layout() {
  gfx::Rect bounds(GetLocalBounds());
  bounds.Inset(kOverviewMargin, kOverviewMargin);

  if (backdrop_view_) {
    gfx::Rect backdrop_bounds = bounds;
    backdrop_bounds.Inset(0, kHeaderHeightDp, 0, 0);
    backdrop_view_->SetBoundsRect(backdrop_bounds);
  }

  if (preview_view_) {
    gfx::Rect preview_bounds = bounds;
    preview_bounds.Inset(0, kHeaderHeightDp, 0, 0);
    preview_bounds.ClampToCenteredSize(preview_view_->CalculatePreferredSize());
    preview_view_->SetBoundsRect(preview_bounds);
  }

  // Position the header at the top. The close button should be right aligned so
  // that the edge of its icon, not the button itself lines up with the margins.
  const gfx::Rect header_bounds(kOverviewMargin, kOverviewMargin,
                                GetLocalBounds().width() - kWindowMargin,
                                kHeaderHeightDp);
  header_view_->SetBoundsRect(header_bounds);
}

}  // namespace ash
