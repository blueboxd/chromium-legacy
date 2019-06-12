// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_preview_view.h"

#include "ash/multi_user/multi_user_window_manager_impl.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/wallpaper/wallpaper_base_view.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/window_state.h"
#include "base/containers/flat_map.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace ash {

namespace {

// The height of the preview view in dips.
// TODO(afakhry): Change the height to be dynamic according to the new specs.
constexpr int kDeskPreviewHeight = 64;

// The desk preview border size in dips.
constexpr int kBorderSize = 2;

// The rounded corner radii, also in dips.
constexpr gfx::RoundedCornersF kCornerRadii(2);

// Holds data about the original desk's layers to determine what we should do
// when we attempt to mirror those layers.
struct LayerData {
  // If true, the layer won't be mirrored in the desk's mirrored contents. For
  // example windows created by overview mode to hold the CaptionContainerView,
  // or minimized windows' layers, should all be skipped.
  bool should_skip_layer = false;

  // If true, we will reset the mirrored layer transform to identity. This is
  // because windows are transformed in overview mode to position them in their
  // places in the overview mode grid. However, in the desk's mini_view, we
  // should show those windows in their original locations on the screen as if
  // overview mode is inactive.
  bool should_reset_transform = false;
};

// Returns true if |window| can be shown in the desk's preview according to its
// multi-profile ownership status (i.e. can only be shown if it belongs to the
// active user).
bool CanShowWindowForMultiProfile(aura::Window* window) {
  MultiUserWindowManager* multi_user_window_manager =
      MultiUserWindowManagerImpl::Get();
  if (!multi_user_window_manager)
    return true;

  const AccountId account_id =
      multi_user_window_manager->GetUserPresentingWindow(window);
  // An empty account ID is returned if the window is presented for all users.
  if (!account_id.is_valid())
    return true;

  return account_id == multi_user_window_manager->CurrentAccountId();
}

// Recursively mirrors |source_layer| and its children and adds them as children
// of |parent|, taking into account the given |layers_data|.
void MirrorLayerTree(ui::Layer* source_layer,
                     ui::Layer* parent,
                     const base::flat_map<ui::Layer*, LayerData>& layers_data) {
  const auto iter = layers_data.find(source_layer);
  const LayerData layer_data =
      iter == layers_data.end() ? LayerData{} : iter->second;
  if (layer_data.should_skip_layer)
    return;

  auto* mirror = source_layer->Mirror().release();
  parent->Add(mirror);

  for (auto* child : source_layer->children())
    MirrorLayerTree(child, mirror, layers_data);

  // Force the mirrored layers to be visible (in order to show windows on
  // inactive desks).
  // TODO(afakhry): See if we need to avoid this in certain cases.
  mirror->SetVisible(true);
  mirror->SetOpacity(1);
  mirror->set_sync_bounds_with_source(true);

  if (layer_data.should_reset_transform)
    mirror->SetTransform(gfx::Transform());
}

// Gathers the needed data about the layers in the subtree rooted at the layer
// of the given |window|, and fills |out_layers_data|.
void GetLayersData(aura::Window* window,
                   base::flat_map<ui::Layer*, LayerData>* out_layers_data) {
  auto& layer_data = (*out_layers_data)[window->layer()];

  // Windows may be explicitly set to be skipped in mini_views such as those
  // created for overview mode purposes.
  // TODO(afakhry): Exclude exo's root surface, since it's a place holder and
  // doesn't have any content. See `exo::SurfaceTreeHost::SetRootSurface()`.
  if (window->GetProperty(kHideInDeskMiniViewKey)) {
    layer_data.should_skip_layer = true;
    return;
  }

  // Minimized windows should not show up in the mini_view.
  auto* window_state = wm::GetWindowState(window);
  if (window_state && window_state->IsMinimized()) {
    layer_data.should_skip_layer = true;
    return;
  }

  if (!CanShowWindowForMultiProfile(window)) {
    layer_data.should_skip_layer = true;
    return;
  }

  // Windows transformed into position in the overview mode grid should be
  // mirrored and the transforms of the mirrored layers should be reset to
  // identity.
  if (window->GetProperty(kIsShowingInOverviewKey))
    layer_data.should_reset_transform = true;

  for (auto* child : window->children())
    GetLayersData(child, out_layers_data);
}

}  // namespace

DeskPreviewView::DeskPreviewView(DeskMiniView* mini_view)
    : mini_view_(mini_view),
      background_view_(new views::View),
      wallpaper_preview_(new DeskWallpaperPreview),
      desk_mirrored_contents_view_(new views::View),
      force_occlusion_tracker_visible_(
          std::make_unique<aura::WindowOcclusionTracker::ScopedForceVisible>(
              mini_view->GetDeskContainer())) {
  DCHECK(mini_view_);

  SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  layer()->SetMasksToBounds(true);

  background_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  auto* background_layer = background_view_->layer();
  background_layer->SetRoundedCornerRadius(kCornerRadii);
  background_layer->SetIsFastRoundedCorner(true);
  AddChildView(background_view_);

  wallpaper_preview_->SetPaintToLayer();
  auto* wallpaper_preview_layer = wallpaper_preview_->layer();
  wallpaper_preview_layer->SetFillsBoundsOpaquely(false);
  wallpaper_preview_layer->SetRoundedCornerRadius(kCornerRadii);
  wallpaper_preview_layer->SetIsFastRoundedCorner(true);
  AddChildView(wallpaper_preview_);

  desk_mirrored_contents_view_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  ui::Layer* contents_view_layer = desk_mirrored_contents_view_->layer();
  contents_view_layer->SetMasksToBounds(true);
  contents_view_layer->set_name("Desk mirrored contents view");
  AddChildView(desk_mirrored_contents_view_);

  RecreateDeskContentsMirrorLayers();
}

DeskPreviewView::~DeskPreviewView() = default;

// static
int DeskPreviewView::GetHeight() {
  return kDeskPreviewHeight;
}

void DeskPreviewView::SetBorderColor(SkColor color) {
  background_view_->layer()->SetColor(color);
}

void DeskPreviewView::RecreateDeskContentsMirrorLayers() {
  auto* desk_container = mini_view_->GetDeskContainer();
  DCHECK(desk_container);
  DCHECK(desk_container->layer());

  // Mirror the layer tree of the desk container.
  auto mirrored_content_root_layer =
      std::make_unique<ui::Layer>(ui::LAYER_NOT_DRAWN);
  mirrored_content_root_layer->set_name("mirrored contents root layer");
  base::flat_map<ui::Layer*, LayerData> layers_data;
  GetLayersData(desk_container, &layers_data);
  MirrorLayerTree(desk_container->layer(), mirrored_content_root_layer.get(),
                  layers_data);

  // Add the root of the mirrored layer tree as a child of the
  // |desk_mirrored_contents_view_|'s layer.
  ui::Layer* contents_view_layer = desk_mirrored_contents_view_->layer();
  contents_view_layer->Add(mirrored_content_root_layer.get());

  // Take ownership of the mirrored layer tree.
  desk_mirrored_contents_layer_tree_owner_ =
      std::make_unique<ui::LayerTreeOwner>(
          std::move(mirrored_content_root_layer));

  Layout();
}

const char* DeskPreviewView::GetClassName() const {
  return "DeskPreviewView";
}

void DeskPreviewView::Layout() {
  gfx::Rect bounds = GetLocalBounds();
  background_view_->SetBoundsRect(bounds);
  bounds.Inset(kBorderSize, kBorderSize);
  wallpaper_preview_->SetBoundsRect(bounds);
  desk_mirrored_contents_view_->SetBoundsRect(bounds);

  // The desk's contents mirrored layer needs to be scaled down so that it fits
  // exactly in the center of the view.
  const auto root_size = mini_view_->root_window()->layer()->size();
  gfx::Transform transform;
  transform.Scale(static_cast<float>(bounds.width()) / root_size.width(),
                  static_cast<float>(bounds.height()) / root_size.height());
  ui::Layer* desk_mirrored_contents_layer =
      desk_mirrored_contents_layer_tree_owner_->root();
  DCHECK(desk_mirrored_contents_layer);
  desk_mirrored_contents_layer->SetTransform(transform);
}

}  // namespace ash
