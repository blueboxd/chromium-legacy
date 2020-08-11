// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/ash_chrome_service_impl.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/chromeos/crosapi/screen_manager_crosapi.h"
#include "chrome/browser/chromeos/crosapi/select_file_ash.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/crosapi/mojom/select_file.mojom.h"

namespace crosapi {

AshChromeServiceImpl::AshChromeServiceImpl(
    mojo::PendingReceiver<mojom::AshChromeService> pending_receiver)
    : receiver_(this, std::move(pending_receiver)),
      screen_manager_crosapi_(std::make_unique<ScreenManagerCrosapi>()) {
  // TODO(hidehiko): Remove non-critical log from here.
  // Currently this is the signal that the connection is established.
  LOG(WARNING) << "AshChromeService connected.";
}

AshChromeServiceImpl::~AshChromeServiceImpl() = default;

void AshChromeServiceImpl::BindSelectFile(
    mojo::PendingReceiver<mojom::SelectFile> receiver) {
  select_file_crosapi_ = std::make_unique<SelectFileAsh>(std::move(receiver));
}

void AshChromeServiceImpl::BindScreenManager(
    mojo::PendingReceiver<mojom::ScreenManager> receiver) {
  screen_manager_crosapi_->BindReceiver(std::move(receiver));
}

}  // namespace crosapi
