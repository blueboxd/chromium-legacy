// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ambient/photo_controller_impl.h"

#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"

PhotoControllerImpl::PhotoControllerImpl() = default;

PhotoControllerImpl::~PhotoControllerImpl() = default;

void PhotoControllerImpl::GetNextImage(
    ash::PhotoController::PhotoDownloadCallback callback) {
  // TODO(wutao): Temporarily use an icon for placeholder. Get images from
  // Backdrop service.
  std::move(callback).Run(gfx::CreateVectorIcon(
      ash::kNotificationAssistantIcon, /*dip_size=*/32, gfx::kGoogleGrey700));
}
