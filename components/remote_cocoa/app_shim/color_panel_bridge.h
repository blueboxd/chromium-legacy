// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_COLOR_PANEL_BRIDGE_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_COLOR_PANEL_BRIDGE_H_

#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"
#include "components/remote_cocoa/common/color_panel.mojom.h"

namespace remote_cocoa {

// A bridge between the mojo ColorPanel interface and the Objective C
// ColorPanelListener.
class REMOTE_COCOA_APP_SHIM_EXPORT ColorPanelBridge
    : public remote_cocoa::mojom::ColorPanel {
 public:
  ColorPanelBridge(mojom::ColorPanelHostPtr host);
  ~ColorPanelBridge() override;
  mojom::ColorPanelHost* host() { return host_.get(); }

  // mojom::ColorPanel.
  void Show(uint32_t initial_color) override;
  void SetSelectedColor(uint32_t color) override;

 private:
  mojom::ColorPanelHostPtr host_;
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_COLOR_PANEL_BRIDGE_H_
