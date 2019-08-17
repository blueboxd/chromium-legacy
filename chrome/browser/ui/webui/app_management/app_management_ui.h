// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_UI_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/webui/app_management/app_management.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/webui/mojo_web_ui_controller.h"

class AppManagementPageHandler;

class AppManagementUI : public ui::MojoWebUIController,
                        public app_management::mojom::PageHandlerFactory {
 public:
  explicit AppManagementUI(content::WebUI* web_ui);
  ~AppManagementUI() override;

  static bool IsEnabled();

 private:
  void BindPageHandlerFactory(
      app_management::mojom::PageHandlerFactoryRequest request);

  // app_management::mojom::PageHandlerFactory:
  void CreatePageHandler(
      app_management::mojom::PagePtr page,
      app_management::mojom::PageHandlerRequest request) override;

  std::unique_ptr<AppManagementPageHandler> page_handler_;
  mojo::Binding<app_management::mojom::PageHandlerFactory>
      page_factory_binding_;

  DISALLOW_COPY_AND_ASSIGN(AppManagementUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_UI_H_
