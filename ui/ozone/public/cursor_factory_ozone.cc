// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/cursor_factory_ozone.h"

#include "base/logging.h"

namespace ui {

namespace {

CursorFactoryOzone* g_instance = nullptr;

}  // namespace

CursorFactoryOzone::CursorFactoryOzone() {
  DCHECK(!g_instance)
      << "There should only be a single CursorFactoryOzone per thread.";
  g_instance = this;
}

CursorFactoryOzone::~CursorFactoryOzone() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

CursorFactoryOzone* CursorFactoryOzone::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

PlatformCursor CursorFactoryOzone::GetDefaultCursor(mojom::CursorType type) {
  NOTIMPLEMENTED();
  return NULL;
}

PlatformCursor CursorFactoryOzone::CreateImageCursor(const SkBitmap& bitmap,
                                                     const gfx::Point& hotspot,
                                                     float bitmap_dpi) {
  NOTIMPLEMENTED();
  return NULL;
}

PlatformCursor CursorFactoryOzone::CreateAnimatedCursor(
    const std::vector<SkBitmap>& bitmaps,
    const gfx::Point& hotspot,
    int frame_delay_ms,
    float bitmap_dpi) {
  NOTIMPLEMENTED();
  return NULL;
}

void CursorFactoryOzone::RefImageCursor(PlatformCursor cursor) {
  NOTIMPLEMENTED();
}

void CursorFactoryOzone::UnrefImageCursor(PlatformCursor cursor) {
  NOTIMPLEMENTED();
}

}  // namespace ui
