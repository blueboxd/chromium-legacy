// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_dialog_controller.h"

SharingDialogController::App::App(std::string icon,
                                  base::string16 name,
                                  std::string identifier)
    : icon(std::move(icon)),
      name(std::move(name)),
      identifier(std::move(identifier)) {}

SharingDialogController::App::App(App&& other) = default;

SharingDialogController::App::~App() = default;
