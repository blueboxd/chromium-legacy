// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_PROBE_SERVICE_H_
#define CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_PROBE_SERVICE_H_

#if defined(OFFICIAL_BUILD)
#error Probe service should only be included in unofficial builds.
#endif

#include "chromeos/components/telemetry_extension_ui/mojom/probe_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {

class ProbeService : public health::mojom::ProbeService {
 public:
  explicit ProbeService(
      mojo::PendingReceiver<health::mojom::ProbeService> receiver);
  ProbeService(const ProbeService&) = delete;
  ProbeService& operator=(const ProbeService&) = delete;
  ~ProbeService() override;

 private:
  mojo::Receiver<health::mojom::ProbeService> receiver_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_PROBE_SERVICE_H_
