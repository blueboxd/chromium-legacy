// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/mojo_web_ui_controller.h"

#ifndef CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_TELEMETRY_EXTENSION_UI_H_
#define CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_TELEMETRY_EXTENSION_UI_H_

#if defined(OFFICIAL_BUILD)
#error Telemetry Extension should only be included in unofficial builds.
#endif

namespace chromeos {

// The WebUI for chrome://telemetry-extension/.
class TelemetryExtensionUI : public ui::MojoWebUIController {
 public:
  explicit TelemetryExtensionUI(content::WebUI* web_ui);
  TelemetryExtensionUI(const TelemetryExtensionUI&) = delete;
  TelemetryExtensionUI& operator=(const TelemetryExtensionUI&) = delete;
  ~TelemetryExtensionUI() override;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_TELEMETRY_EXTENSION_UI_H_
