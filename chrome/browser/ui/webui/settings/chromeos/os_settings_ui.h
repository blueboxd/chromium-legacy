// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_UI_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebUIMessageHandler;
}

namespace chromeos {
namespace settings {

// The WebUI handler for chrome://os-settings.
class OSSettingsUI : public ui::MojoWebUIController {
 public:
  explicit OSSettingsUI(content::WebUI* web_ui);
  ~OSSettingsUI() override;

 private:
  void AddSettingsPageUIHandler(
      std::unique_ptr<content::WebUIMessageHandler> handler);
  void BindCrosNetworkConfig(
      network_config::mojom::CrosNetworkConfigRequest request);

  // TODO(crbug/950007): Create load histograms and embed WebuiLoadTimer.

  DISALLOW_COPY_AND_ASSIGN(OSSettingsUI);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_UI_H_
