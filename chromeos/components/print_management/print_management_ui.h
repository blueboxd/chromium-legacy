// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/mojo_web_ui_controller.h"

#ifndef CHROMEOS_COMPONENTS_PRINT_MANAGEMENT_PRINT_MANAGEMENT_UI_H_
#define CHROMEOS_COMPONENTS_PRINT_MANAGEMENT_PRINT_MANAGEMENT_UI_H_

namespace chromeos {

// The WebUI for chrome://print-management/.
class PrintManagementUI : public ui::MojoWebUIController {
 public:
  explicit PrintManagementUI(content::WebUI* web_ui);
  ~PrintManagementUI() override;

  PrintManagementUI(const PrintManagementUI&) = delete;
  PrintManagementUI& operator=(const PrintManagementUI&) = delete;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PRINT_MANAGEMENT_PRINT_MANAGEMENT_UI_H_
