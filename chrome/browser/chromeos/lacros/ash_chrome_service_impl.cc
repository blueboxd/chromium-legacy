// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lacros/ash_chrome_service_impl.h"

#include <utility>

#include "base/logging.h"

namespace chromeos {

AshChromeServiceImpl::AshChromeServiceImpl(
    mojo::PendingReceiver<lacros::mojom::AshChromeService> pending_receiver)
    : receiver_(this, std::move(pending_receiver)) {
  // TODO(hidehiko): Remove non-critical log from here.
  // Currently this is the signal that the connection is established.
  LOG(WARNING) << "AshChromeService connected.";
}

AshChromeServiceImpl::~AshChromeServiceImpl() = default;

}  // namespace chromeos
