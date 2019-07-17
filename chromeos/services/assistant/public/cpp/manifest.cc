// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "chromeos/services/assistant/public/mojom/constants.mojom.h"
#include "chromeos/services/assistant/public/mojom/settings.mojom.h"
#include "chromeos/services/network_config/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace chromeos {
namespace assistant {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Assistant Service")
          .WithOptions(service_manager::ManifestOptionsBuilder().Build())
          .ExposeCapability("assistant",
                            service_manager::Manifest::InterfaceList<
                                mojom::Assistant, mojom::AssistantPlatform,
                                mojom::AssistantSettingsManager>())
          .RequireCapability("ash", "system_ui")
          .RequireCapability("assistant_audio_decoder",
                             "assistant:audio_decoder")
          .RequireCapability("audio", "stream_factory")
          .RequireCapability("device", "device:battery_monitor")
          .RequireCapability("device", "device:wake_lock")
          .RequireCapability("identity", "identity_accessor")
          .RequireCapability("media_session", "app")
          .RequireCapability(
              chromeos::network_config::mojom::kServiceName,
              chromeos::network_config::mojom::kNetworkConfigCapability)
          .RequireCapability("preferences", "pref_client")

          .Build()};
  return *manifest;
}

}  // namespace assistant
}  // namespace chromeos
