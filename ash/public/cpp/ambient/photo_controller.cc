// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ambient/photo_controller.h"

#include "base/check_op.h"

namespace ash {

namespace {

PhotoController* g_photo_controller = nullptr;

}  // namespace

// Topic-----------------------------------------------------------------------
PhotoController::Topic::Topic() = default;

PhotoController::Topic::Topic(const Topic&) = default;

PhotoController::Topic& PhotoController::Topic::operator=(const Topic&) =
    default;

PhotoController::Topic::~Topic() = default;

// WeatherInfo-----------------------------------------------------------------
PhotoController::WeatherInfo::WeatherInfo() = default;

PhotoController::WeatherInfo::WeatherInfo(const WeatherInfo&) = default;

PhotoController::WeatherInfo& PhotoController::WeatherInfo::operator=(
    const WeatherInfo&) = default;

PhotoController::WeatherInfo::~WeatherInfo() = default;

// ScreenUpdate----------------------------------------------------------------
PhotoController::ScreenUpdate::ScreenUpdate() = default;

PhotoController::ScreenUpdate::ScreenUpdate(const ScreenUpdate&) = default;

PhotoController::ScreenUpdate& PhotoController::ScreenUpdate::operator=(
    const ScreenUpdate&) = default;

PhotoController::ScreenUpdate::~ScreenUpdate() = default;

// static
PhotoController* PhotoController::Get() {
  return g_photo_controller;
}

PhotoController::PhotoController() {
  DCHECK(!g_photo_controller);
  g_photo_controller = this;
}

PhotoController::~PhotoController() {
  DCHECK_EQ(g_photo_controller, this);
  g_photo_controller = nullptr;
}

}  // namespace ash
