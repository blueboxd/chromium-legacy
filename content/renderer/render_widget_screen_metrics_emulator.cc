// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget_screen_metrics_emulator.h"

#include "content/public/common/context_menu_params.h"
#include "content/renderer/render_widget_screen_metrics_emulator_delegate.h"

namespace content {

RenderWidgetScreenMetricsEmulator::RenderWidgetScreenMetricsEmulator(
    RenderWidgetScreenMetricsEmulatorDelegate* delegate,
    const ScreenInfo& screen_info,
    const gfx::Size& widget_size,
    const gfx::Size& visible_viewport_size,
    const gfx::Rect& view_screen_rect,
    const gfx::Rect& window_screen_rect)
    : delegate_(delegate),
      original_screen_info_(screen_info),
      original_widget_size_(widget_size),
      original_visible_viewport_size_(visible_viewport_size),
      original_view_screen_rect_(view_screen_rect),
      original_window_screen_rect_(window_screen_rect) {}

RenderWidgetScreenMetricsEmulator::~RenderWidgetScreenMetricsEmulator() =
    default;

void RenderWidgetScreenMetricsEmulator::DisableAndApply() {
  delegate_->SetScreenMetricsEmulationParameters(false, emulation_params_);
  delegate_->SetScreenRects(original_view_screen_rect_,
                            original_window_screen_rect_);
  delegate_->SetScreenInfoAndSize(original_screen_info_, original_widget_size_,
                                  original_visible_viewport_size_);
}

void RenderWidgetScreenMetricsEmulator::ChangeEmulationParams(
    const blink::WebDeviceEmulationParams& params) {
  emulation_params_ = params;
  Apply();
}

void RenderWidgetScreenMetricsEmulator::Apply() {
  ScreenInfo screen_info = original_screen_info_;

  applied_widget_rect_.set_size(gfx::Size(emulation_params_.view_size));

  if (!applied_widget_rect_.width())
    applied_widget_rect_.set_width(original_size().width());

  if (!applied_widget_rect_.height())
    applied_widget_rect_.set_height(original_size().height());

  scale_ = emulation_params_.scale;
  if (!emulation_params_.view_size.width &&
      !emulation_params_.view_size.height && scale_) {
    applied_widget_rect_.set_size(
        gfx::ScaleToRoundedSize(original_size(), 1.f / scale_));
  }

  gfx::Rect window_screen_rect;
  if (emulation_params_.screen_position ==
      blink::WebDeviceEmulationParams::kDesktop) {
    screen_info.rect = original_screen_info().rect;
    screen_info.available_rect = original_screen_info().available_rect;
    window_screen_rect = original_window_screen_rect_;
    if (emulation_params_.view_position) {
      applied_widget_rect_.set_origin(*emulation_params_.view_position);
      window_screen_rect.set_origin(*emulation_params_.view_position);
    } else {
      applied_widget_rect_.set_origin(original_view_screen_rect_.origin());
    }
  } else {
    applied_widget_rect_.set_origin(
        emulation_params_.view_position.value_or(blink::WebPoint()));
    screen_info.rect = applied_widget_rect_;
    screen_info.available_rect = applied_widget_rect_;
    window_screen_rect = applied_widget_rect_;
  }

  if (!emulation_params_.screen_size.IsEmpty()) {
    gfx::Rect screen_rect = gfx::Rect(0, 0, emulation_params_.screen_size.width,
                                      emulation_params_.screen_size.height);
    screen_info.rect = screen_rect;
    screen_info.available_rect = screen_rect;
  }

  screen_info.device_scale_factor =
      emulation_params_.device_scale_factor
          ? emulation_params_.device_scale_factor
          : original_screen_info().device_scale_factor;

  if (emulation_params_.screen_orientation_type !=
      blink::kWebScreenOrientationUndefined) {
    switch (emulation_params_.screen_orientation_type) {
      case blink::kWebScreenOrientationPortraitPrimary:
        screen_info.orientation_type =
            SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY;
        break;
      case blink::kWebScreenOrientationPortraitSecondary:
        screen_info.orientation_type =
            SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY;
        break;
      case blink::kWebScreenOrientationLandscapePrimary:
        screen_info.orientation_type =
            SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY;
        break;
      case blink::kWebScreenOrientationLandscapeSecondary:
        screen_info.orientation_type =
            SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY;
        break;
      default:
        screen_info.orientation_type = SCREEN_ORIENTATION_VALUES_DEFAULT;
        break;
    }
    screen_info.orientation_angle = emulation_params_.screen_orientation_angle;
  }

  // Pass three emulation parameters to the blink side:
  // - we keep the real device scale factor in compositor to produce sharp image
  //   even when emulating different scale factor;
  // - in order to fit into view, WebView applies offset and scale to the
  //   root layer.
  blink::WebDeviceEmulationParams modified_emulation_params = emulation_params_;
  modified_emulation_params.device_scale_factor =
      original_screen_info().device_scale_factor;
  modified_emulation_params.scale = scale_;
  delegate_->SetScreenMetricsEmulationParameters(true,
                                                 modified_emulation_params);

  delegate_->SetScreenRects(applied_widget_rect_, window_screen_rect);

  delegate_->SetScreenInfoAndSize(
      screen_info, /*widget_size=*/applied_widget_rect_.size(),
      /*visible_viewport_size=*/applied_widget_rect_.size());
}

void RenderWidgetScreenMetricsEmulator::OnSynchronizeVisualProperties(
    const ScreenInfo& screen_info,
    const gfx::Size& widget_size,
    const gfx::Size& visible_viewport_size) {
  original_screen_info_ = screen_info;
  original_widget_size_ = widget_size;
  original_visible_viewport_size_ = visible_viewport_size;
  Apply();
}

void RenderWidgetScreenMetricsEmulator::OnUpdateScreenRects(
    const gfx::Rect& view_screen_rect,
    const gfx::Rect& window_screen_rect) {
  original_view_screen_rect_ = view_screen_rect;
  original_window_screen_rect_ = window_screen_rect;
  if (emulation_params_.screen_position ==
      blink::WebDeviceEmulationParams::kDesktop) {
    Apply();
  }
}

void RenderWidgetScreenMetricsEmulator::OnShowContextMenu(
    ContextMenuParams* params) {
  params->x *= scale_;
  params->y *= scale_;
}

}  // namespace content
