// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/overlay_panel_layer.h"

#include "cc/layers/layer.h"
#include "cc/layers/nine_patch_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/ui_resource_layer.h"
#include "cc/resources/scoped_ui_resource.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/resources/nine_patch_resource.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/base/l10n/l10n_util_android.h"
#include "ui/gfx/color_utils.h"

namespace android {

const float OverlayPanelLayer::kDefaultIconWidthDp = 36.0f;
const int OverlayPanelLayer::kInvalidResourceID = -1;

scoped_refptr<cc::Layer> OverlayPanelLayer::GetIconLayer() {
  if (panel_icon_resource_id_ == -1)
    return nullptr;
  ui::Resource* panel_icon_resource = resource_manager_->GetResource(
      ui::ANDROID_RESOURCE_TYPE_STATIC, panel_icon_resource_id_);
  DCHECK(panel_icon_resource);

  if (panel_icon_->parent() != layer_) {
    layer_->AddChild(panel_icon_);
  }

  panel_icon_->SetUIResourceId(panel_icon_resource->ui_resource()->id());
  panel_icon_->SetBounds(panel_icon_resource->size());

  return panel_icon_;
}

void OverlayPanelLayer::AddBarTextLayer(scoped_refptr<cc::Layer> text_layer) {
  text_container_->AddChild(text_layer);
}

void OverlayPanelLayer::SetResourceIds(int bar_text_resource_id,
                                       int panel_shadow_resource_id,
                                       int bar_shadow_resource_id,
                                       int panel_icon_resource_id,
                                       int drag_handlebar_resource_id,
                                       int open_tab_icon_resource_id,
                                       int close_icon_resource_id) {
  bar_text_resource_id_ = bar_text_resource_id;
  panel_shadow_resource_id_ = panel_shadow_resource_id;
  bar_shadow_resource_id_ = bar_shadow_resource_id;
  panel_icon_resource_id_ = panel_icon_resource_id;
  drag_handlebar_resource_id_ = drag_handlebar_resource_id;
  open_tab_icon_resource_id_ = open_tab_icon_resource_id;
  close_icon_resource_id_ = close_icon_resource_id;
}

void OverlayPanelLayer::SetProperties(
    float dp_to_px,
    const scoped_refptr<cc::Layer>& content_layer,
    float content_offset_y,
    float panel_x,
    float panel_y,
    float panel_width,
    float panel_height,
    int bar_background_color,
    float bar_margin_side,
    float bar_margin_top,
    float bar_height,
    float bar_offset_y,
    float bar_text_opacity,
    bool bar_border_visible,
    float bar_border_height,
    bool bar_shadow_visible,
    float bar_shadow_opacity,
    int icon_tint,
    int drag_handlebar_tint,
    float icon_opacity) {
  // Grabs required static resources.
  ui::NinePatchResource* panel_shadow_resource =
      ui::NinePatchResource::From(resource_manager_->GetResource(
          ui::ANDROID_RESOURCE_TYPE_STATIC, panel_shadow_resource_id_));

  DCHECK(panel_shadow_resource);

  // Round values to avoid pixel gap between layers.
  bar_height = floor(bar_height);

  float bar_top = bar_offset_y;
  float bar_bottom = bar_top + bar_height;

  bool is_rtl = l10n_util::IsLayoutRtl();

  // ---------------------------------------------------------------------------
  // Panel Shadow
  // ---------------------------------------------------------------------------
  gfx::Size shadow_res_size = panel_shadow_resource->size();
  gfx::Rect shadow_res_padding = panel_shadow_resource->padding();
  gfx::Size shadow_bounds(
      panel_width + shadow_res_size.width()
          - shadow_res_padding.size().width(),
      panel_height + shadow_res_size.height()
          - shadow_res_padding.size().height());
  panel_shadow_->SetUIResourceId(panel_shadow_resource->ui_resource()->id());
  panel_shadow_->SetBorder(panel_shadow_resource->Border(shadow_bounds));
  panel_shadow_->SetAperture(panel_shadow_resource->aperture());
  panel_shadow_->SetBounds(shadow_bounds);
  gfx::PointF shadow_position(-shadow_res_padding.origin().x(),
                              -shadow_res_padding.origin().y());
  panel_shadow_->SetPosition(shadow_position);

  // ---------------------------------------------------------------------------
  // Bar Background
  // ---------------------------------------------------------------------------
  gfx::Size background_size(panel_width, bar_height);
  bar_background_->SetBounds(background_size);
  bar_background_->SetPosition(gfx::PointF(0.f, bar_top));
  bar_background_->SetBackgroundColor(bar_background_color);

  // ---------------------------------------------------------------------------
  // Bar Text
  // ---------------------------------------------------------------------------
  ui::Resource* bar_text_resource = resource_manager_->GetResource(
      ui::ANDROID_RESOURCE_TYPE_DYNAMIC, bar_text_resource_id_);

  if (bar_text_resource) {
    // Centers the text vertically in the Search Bar.
    float bar_padding_top =
        bar_top + bar_height / 2 - bar_text_resource->size().height() / 2;
    bar_text_->SetUIResourceId(bar_text_resource->ui_resource()->id());
    bar_text_->SetBounds(bar_text_resource->size());
    bar_text_->SetPosition(gfx::PointF(0.f, bar_padding_top));
    bar_text_->SetOpacity(bar_text_opacity);
  }

  // ---------------------------------------------------------------------------
  // Panel Icon
  // ---------------------------------------------------------------------------
  scoped_refptr<cc::Layer> icon_layer = GetIconLayer();
  if (icon_layer) {
    // If the icon is not the default width, add or remove padding so it appears
    // centered.
    float icon_padding = (kDefaultIconWidthDp * dp_to_px -
        icon_layer->bounds().width()) / 2.0f;

    // Positions the Icon at the start of the bar.
    float icon_x;
    if (is_rtl) {
      icon_x = panel_width - icon_layer->bounds().width() -
          (bar_margin_side + icon_padding);
    } else {
      icon_x = bar_margin_side + icon_padding;
    }

    // Centers the Icon vertically in the bar.
    float icon_y = bar_top + bar_height / 2 - icon_layer->bounds().height() / 2;

    icon_layer->SetPosition(gfx::PointF(icon_x, icon_y));
  }

  // ---------------------------------------------------------------------------
  // Drag Handlebar
  // ---------------------------------------------------------------------------
  if (drag_handlebar_resource_id_ != 0 && drag_handlebar_resource_id_ != -1) {
    ui::Resource* drag_handlebar_resource =
        resource_manager_->GetStaticResourceWithTint(
            drag_handlebar_resource_id_, drag_handlebar_tint);
    drag_handlebar_->SetUIResourceId(
        drag_handlebar_resource->ui_resource()->id());
    drag_handlebar_->SetBounds(drag_handlebar_resource->size());
    float drag_handlebar_left =
        panel_width / 2 - drag_handlebar_resource->size().width() / 2;
    float drag_handlebar_top =
        bar_top + bar_margin_top - drag_handlebar_resource->size().height() / 2;
    drag_handlebar_->SetPosition(
        gfx::PointF(drag_handlebar_left, drag_handlebar_top));
  }

  // ---------------------------------------------------------------------------
  // Close Icon
  // ---------------------------------------------------------------------------
  // Grab the Close Icon resource.
  ui::Resource* close_icon_resource =
      resource_manager_->GetStaticResourceWithTint(close_icon_resource_id_,
                                                   icon_tint);

  // Positions the icon at the end of the bar.
  float close_icon_left;
  if (is_rtl) {
    close_icon_left = bar_margin_side;
  } else {
    close_icon_left =
        panel_width - close_icon_resource->size().width() - bar_margin_side;
  }

  // Centers the Close Icon vertically in the bar.
  float close_icon_top =
      bar_top + bar_height / 2 - close_icon_resource->size().height() / 2;

  close_icon_->SetUIResourceId(close_icon_resource->ui_resource()->id());
  close_icon_->SetBounds(close_icon_resource->size());
  close_icon_->SetPosition(
      gfx::PointF(close_icon_left, close_icon_top));
  close_icon_->SetOpacity(icon_opacity);

  // ---------------------------------------------------------------------------
  // Open Tab icon
  // ---------------------------------------------------------------------------
  if (open_tab_icon_resource_id_ != kInvalidResourceID) {
    ui::Resource* open_tab_icon_resource =
        resource_manager_->GetStaticResourceWithTint(open_tab_icon_resource_id_,
                                                     icon_tint);

    // Positions the icon at the end of the bar.
    float open_tab_top = close_icon_top;
    float open_tab_left;
    float spacing_between_icons = 2 * bar_margin_side;
    float margin_from_close_icon =
        close_icon_resource->size().width() + spacing_between_icons;
    if (is_rtl) {
      open_tab_left = close_icon_left + margin_from_close_icon;
    } else {
      open_tab_left = close_icon_left - margin_from_close_icon;
    }

    open_tab_icon_->SetUIResourceId(
        open_tab_icon_resource->ui_resource()->id());
    open_tab_icon_->SetBounds(open_tab_icon_resource->size());
    open_tab_icon_->SetPosition(gfx::PointF(open_tab_left, open_tab_top));
    open_tab_icon_->SetOpacity(icon_opacity);
  }

  // ---------------------------------------------------------------------------
  // Content
  // ---------------------------------------------------------------------------
  content_container_->SetPosition(
      gfx::PointF(0.f, content_offset_y));
  content_container_->SetBounds(gfx::Size(panel_width, panel_height));
  content_container_->SetBackgroundColor(bar_background_color);
  if (content_layer) {
    if (content_layer->parent() != content_container_)
      content_container_->AddChild(content_layer);
  } else {
    content_container_->RemoveAllChildren();
  }

  // ---------------------------------------------------------------------------
  // Bar Shadow
  // ---------------------------------------------------------------------------
  if (bar_shadow_visible) {
    ui::Resource* bar_shadow_resource = resource_manager_->GetResource(
        ui::ANDROID_RESOURCE_TYPE_STATIC, bar_shadow_resource_id_);

    if (bar_shadow_resource) {
      if (bar_shadow_->parent() != layer_)
        layer_->AddChild(bar_shadow_);

      int shadow_height = bar_shadow_resource->size().height();
      gfx::Size shadow_size(panel_width, shadow_height);

      bar_shadow_->SetUIResourceId(bar_shadow_resource->ui_resource()->id());
      bar_shadow_->SetBounds(shadow_size);
      bar_shadow_->SetPosition(gfx::PointF(0.f, bar_bottom));
      bar_shadow_->SetOpacity(bar_shadow_opacity);
    }
  } else {
    if (bar_shadow_.get() && bar_shadow_->parent())
      bar_shadow_->RemoveFromParent();
  }

  // ---------------------------------------------------------------------------
  // Panel
  // ---------------------------------------------------------------------------
  layer_->SetPosition(gfx::PointF(panel_x, panel_y));

  // ---------------------------------------------------------------------------
  // Bar border
  // ---------------------------------------------------------------------------
  if (bar_border_visible) {
    gfx::Size bar_border_size(panel_width,
                                     bar_border_height);
    float border_y = bar_bottom - bar_border_height;
    bar_border_->SetBounds(bar_border_size);
    bar_border_->SetPosition(
        gfx::PointF(0.f, border_y));
    bar_border_->SetBackgroundColor(bar_background_color);
    layer_->AddChild(bar_border_);
  } else if (bar_border_.get() && bar_border_->parent()) {
    bar_border_->RemoveFromParent();
  }
}

void OverlayPanelLayer::SetProgressBar(int progress_bar_background_resource_id,
                                       int progress_bar_resource_id,
                                       bool progress_bar_visible,
                                       float progress_bar_position_y,
                                       float progress_bar_height,
                                       float progress_bar_opacity,
                                       int progress_bar_completion,
                                       float panel_width) {
  bool should_render_progress_bar =
      progress_bar_visible && progress_bar_opacity > 0.f;

  if (should_render_progress_bar) {
    ui::NinePatchResource* progress_bar_background_resource =
        ui::NinePatchResource::From(resource_manager_->GetResource(
            ui::ANDROID_RESOURCE_TYPE_STATIC,
            progress_bar_background_resource_id));
    ui::NinePatchResource* progress_bar_resource =
        ui::NinePatchResource::From(resource_manager_->GetResource(
            ui::ANDROID_RESOURCE_TYPE_STATIC, progress_bar_resource_id));

    DCHECK(progress_bar_background_resource);
    DCHECK(progress_bar_resource);

    // Progress Bar Background
    if (progress_bar_background_->parent() != layer_)
      layer_->AddChild(progress_bar_background_);

    float progress_bar_y = progress_bar_position_y - progress_bar_height;
    gfx::Size progress_bar_background_size(panel_width, progress_bar_height);

    progress_bar_background_->SetUIResourceId(
        progress_bar_background_resource->ui_resource()->id());
    progress_bar_background_->SetBorder(
        progress_bar_background_resource->Border(progress_bar_background_size));
    progress_bar_background_->SetAperture(
        progress_bar_background_resource->aperture());
    progress_bar_background_->SetBounds(progress_bar_background_size);
    progress_bar_background_->SetPosition(gfx::PointF(0.f, progress_bar_y));
    progress_bar_background_->SetOpacity(progress_bar_opacity);

    // Progress Bar
    if (progress_bar_->parent() != layer_)
      layer_->AddChild(progress_bar_);

    float progress_bar_width =
        floor(panel_width * progress_bar_completion / 100.f);
    gfx::Size progress_bar_size(progress_bar_width, progress_bar_height);
    progress_bar_->SetUIResourceId(progress_bar_resource->ui_resource()->id());
    progress_bar_->SetBorder(progress_bar_resource->Border(progress_bar_size));
    progress_bar_->SetAperture(progress_bar_resource->aperture());
    progress_bar_->SetBounds(progress_bar_size);
    progress_bar_->SetPosition(gfx::PointF(0.f, progress_bar_y));
    progress_bar_->SetOpacity(progress_bar_opacity);
  } else {
    // Removes Progress Bar and its Background from the Layer Tree.
    if (progress_bar_background_.get() && progress_bar_background_->parent())
      progress_bar_background_->RemoveFromParent();

    if (progress_bar_.get() && progress_bar_->parent())
      progress_bar_->RemoveFromParent();
  }
}

OverlayPanelLayer::OverlayPanelLayer(ui::ResourceManager* resource_manager)
    : resource_manager_(resource_manager),
      layer_(cc::Layer::Create()),
      panel_shadow_(cc::NinePatchLayer::Create()),
      bar_background_(cc::SolidColorLayer::Create()),
      bar_text_(cc::UIResourceLayer::Create()),
      bar_shadow_(cc::UIResourceLayer::Create()),
      panel_icon_(cc::UIResourceLayer::Create()),
      drag_handlebar_(cc::UIResourceLayer::Create()),
      open_tab_icon_(cc::UIResourceLayer::Create()),
      close_icon_(cc::UIResourceLayer::Create()),
      content_container_(cc::SolidColorLayer::Create()),
      text_container_(cc::Layer::Create()),
      bar_border_(cc::SolidColorLayer::Create()),
      progress_bar_(cc::NinePatchLayer::Create()),
      progress_bar_background_(cc::NinePatchLayer::Create()) {
  // Background colors for each widget are set in SetProperties, where variable
  // colors are available.
  layer_->SetMasksToBounds(false);
  layer_->SetIsDrawable(true);

  // Panel Shadow
  panel_shadow_->SetIsDrawable(true);
  panel_shadow_->SetFillCenter(false);
  layer_->AddChild(panel_shadow_);

  // Bar Background
  bar_background_->SetIsDrawable(true);
  layer_->AddChild(bar_background_);

  // Bar Text
  bar_text_->SetIsDrawable(true);
  AddBarTextLayer(bar_text_);
  layer_->AddChild(text_container_);

  // Panel Icon
  panel_icon_->SetIsDrawable(true);

  // The container that any text in the bar will be added to.
  text_container_->SetIsDrawable(true);

  // Drag Handlebar
  drag_handlebar_->SetIsDrawable(true);
  layer_->AddChild(drag_handlebar_);

  // Open Tab Icon
  open_tab_icon_->SetIsDrawable(true);
  layer_->AddChild(open_tab_icon_);

  // Close Icon
  close_icon_->SetIsDrawable(true);
  layer_->AddChild(close_icon_);

  // Content Container
  content_container_->SetIsDrawable(true);
  layer_->AddChild(content_container_);

  // Bar Border
  bar_border_->SetIsDrawable(true);

  // Bar Shadow
  bar_shadow_->SetIsDrawable(true);

  // Progress Bar Background
  progress_bar_background_->SetIsDrawable(true);
  progress_bar_background_->SetFillCenter(true);

  // Progress Bar
  progress_bar_->SetIsDrawable(true);
  progress_bar_->SetFillCenter(true);
}

OverlayPanelLayer::~OverlayPanelLayer() {
}

scoped_refptr<cc::Layer> OverlayPanelLayer::layer() {
  return layer_;
}

}  //  namespace android
