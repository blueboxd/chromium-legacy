// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_MOJO_WEB_UI_CONTROLLER_H_
#define UI_WEBUI_MOJO_WEB_UI_CONTROLLER_H_

#include "base/macros.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"

namespace ui {

// MojoWebUIController is intended for WebUI pages that use Mojo. It is
// expected that subclasses will:
// . Add all Mojo Bindings Resources via AddResourcePath(), eg:
//     source-> AddResourcePath("chrome/browser/ui/webui/omnibox/omnibox.mojom",
//                              IDR_OMNIBOX_MOJO_JS);
// . Overload void BindInterface(mojo::PendingReceiver<InterfaceName>) for all
//   Mojo Interfaces it wishes to handle.
class MojoWebUIController : public content::WebUIController {
 public:
  // By default MojoWebUIControllers do not have normal WebUI bindings. Pass
  // |enable_chrome_send| as true if these are needed.
  explicit MojoWebUIController(content::WebUI* contents,
                               bool enable_chrome_send = false);
  ~MojoWebUIController() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MojoWebUIController);
};

}  // namespace ui

#endif  // UI_WEBUI_MOJO_WEB_UI_CONTROLLER_H_
