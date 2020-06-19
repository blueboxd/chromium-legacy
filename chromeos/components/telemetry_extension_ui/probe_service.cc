// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/telemetry_extension_ui/probe_service.h"

#include <utility>

namespace chromeos {

ProbeService::ProbeService(
    mojo::PendingReceiver<health::mojom::ProbeService> receiver)
    : receiver_(this, std::move(receiver)) {}

ProbeService::~ProbeService() = default;

}  // namespace chromeos
