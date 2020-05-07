// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/image_cursors.h"

#include <float.h>
#include <stddef.h>

#include "base/check.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_loader.h"
#include "ui/base/cursor/cursors_aura.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

namespace {

const mojom::CursorType kImageCursorIds[] = {
    mojom::CursorType::kNull,
    mojom::CursorType::kPointer,
    mojom::CursorType::kNoDrop,
    mojom::CursorType::kNotAllowed,
    mojom::CursorType::kCopy,
    mojom::CursorType::kHand,
    mojom::CursorType::kMove,
    mojom::CursorType::kNorthEastResize,
    mojom::CursorType::kSouthWestResize,
    mojom::CursorType::kSouthEastResize,
    mojom::CursorType::kNorthWestResize,
    mojom::CursorType::kNorthResize,
    mojom::CursorType::kSouthResize,
    mojom::CursorType::kEastResize,
    mojom::CursorType::kWestResize,
    mojom::CursorType::kIBeam,
    mojom::CursorType::kAlias,
    mojom::CursorType::kCell,
    mojom::CursorType::kContextMenu,
    mojom::CursorType::kCross,
    mojom::CursorType::kHelp,
    mojom::CursorType::kVerticalText,
    mojom::CursorType::kZoomIn,
    mojom::CursorType::kZoomOut,
    mojom::CursorType::kRowResize,
    mojom::CursorType::kColumnResize,
    mojom::CursorType::kEastWestResize,
    mojom::CursorType::kNorthSouthResize,
    mojom::CursorType::kNorthEastSouthWestResize,
    mojom::CursorType::kNorthWestSouthEastResize,
    mojom::CursorType::kGrab,
    mojom::CursorType::kGrabbing,
};

const mojom::CursorType kAnimatedCursorIds[] = {mojom::CursorType::kWait,
                                                mojom::CursorType::kProgress};

}  // namespace

ImageCursors::ImageCursors() : cursor_size_(CursorSize::kNormal) {}

ImageCursors::~ImageCursors() {
}

void ImageCursors::Initialize() {
  if (!cursor_loader_)
    cursor_loader_.reset(CursorLoader::Create());
}

float ImageCursors::GetScale() const {
  if (!cursor_loader_) {
    NOTREACHED();
    // Returning default on release build as it's not serious enough to crash
    // even if this ever happens.
    return 1.0f;
  }
  return cursor_loader_->scale();
}

display::Display::Rotation ImageCursors::GetRotation() const {
  if (!cursor_loader_) {
    NOTREACHED();
    // Returning default on release build as it's not serious enough to crash
    // even if this ever happens.
    return display::Display::ROTATE_0;
  }
  return cursor_loader_->rotation();
}

bool ImageCursors::SetDisplay(const display::Display& display,
                              float scale_factor) {
  if (!cursor_loader_) {
    cursor_loader_.reset(CursorLoader::Create());
  } else if (cursor_loader_->rotation() == display.panel_rotation() &&
             cursor_loader_->scale() == scale_factor) {
    return false;
  }

  cursor_loader_->set_rotation(display.panel_rotation());
  cursor_loader_->set_scale(scale_factor);
  ReloadCursors();
  return true;
}

void ImageCursors::ReloadCursors() {
  float device_scale_factor = cursor_loader_->scale();

  cursor_loader_->UnloadAll();

  for (size_t i = 0; i < base::size(kImageCursorIds); ++i) {
    int resource_id = -1;
    gfx::Point hot_point;
    bool success =
        GetCursorDataFor(cursor_size_, kImageCursorIds[i], device_scale_factor,
                         &resource_id, &hot_point);
    DCHECK(success);
    cursor_loader_->LoadImageCursor(kImageCursorIds[i], resource_id, hot_point);
  }
  for (size_t i = 0; i < base::size(kAnimatedCursorIds); ++i) {
    int resource_id = -1;
    gfx::Point hot_point;
    bool success =
        GetAnimatedCursorDataFor(cursor_size_, kAnimatedCursorIds[i],
                                 device_scale_factor, &resource_id, &hot_point);
    DCHECK(success);
    cursor_loader_->LoadAnimatedCursor(kAnimatedCursorIds[i],
                                       resource_id,
                                       hot_point,
                                       kAnimatedCursorFrameDelayMs);
  }
}

void ImageCursors::SetCursorSize(CursorSize cursor_size) {
  if (cursor_size_ == cursor_size)
    return;

  cursor_size_ = cursor_size;

  if (cursor_loader_.get())
    ReloadCursors();
}

void ImageCursors::SetPlatformCursor(gfx::NativeCursor* cursor) {
  cursor_loader_->SetPlatformCursor(cursor);
}

base::WeakPtr<ImageCursors> ImageCursors::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ui
