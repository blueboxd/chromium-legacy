// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/ash_chrome_service_impl.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/chromeos/crosapi/screen_manager_crosapi.h"
#include "chrome/browser/chromeos/crosapi/select_file_crosapi.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/crosapi/mojom/select_file.mojom.h"

AshChromeServiceImpl::AshChromeServiceImpl(
    mojo::PendingReceiver<crosapi::mojom::AshChromeService> pending_receiver)
    : receiver_(this, std::move(pending_receiver)),
      screen_manager_crosapi_(std::make_unique<ScreenManagerCrosapi>()) {
  // TODO(hidehiko): Remove non-critical log from here.
  // Currently this is the signal that the connection is established.
  LOG(WARNING) << "AshChromeService connected.";
}

AshChromeServiceImpl::~AshChromeServiceImpl() = default;

void AshChromeServiceImpl::BindSelectFile(
    mojo::PendingReceiver<crosapi::mojom::SelectFile> receiver) {
  select_file_crosapi_ =
      std::make_unique<SelectFileCrosapi>(std::move(receiver));
}

void AshChromeServiceImpl::BindScreenManager(
    mojo::PendingReceiver<crosapi::mojom::ScreenManager> receiver) {
  screen_manager_crosapi_->BindReceiver(std::move(receiver));
}
