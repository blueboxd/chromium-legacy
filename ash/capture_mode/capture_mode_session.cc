// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_session.h"

#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/window.h"

namespace ash {

CaptureModeSession::CaptureModeSession(aura::Window* root)
    : capture_mode_bar_view_(new CaptureModeBarView()) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = root->GetChildById(kShellWindowId_OverlayContainer);
  params.bounds = CaptureModeBarView::GetBounds(root);
  params.name = "CaptureModeBarWidget";

  capture_mode_bar_widget_.Init(std::move(params));
  capture_mode_bar_widget_.SetContentsView(
      base::WrapUnique(capture_mode_bar_view_));
  capture_mode_bar_widget_.Show();
}

CaptureModeSession::~CaptureModeSession() = default;

void CaptureModeSession::OnCaptureSourceChanged(CaptureModeSource new_source) {
  capture_mode_bar_view_->OnCaptureSourceChanged(new_source);
}

void CaptureModeSession::OnCaptureTypeChanged(CaptureModeType new_type) {
  capture_mode_bar_view_->OnCaptureTypeChanged(new_type);
}

}  // namespace ash
