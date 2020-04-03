// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/print_management/print_management_ui.h"

#include "base/memory/ptr_util.h"
#include "chromeos/components/print_management/mojom/printing_manager.mojom.h"
#include "chromeos/components/print_management/url_constants.h"
#include "chromeos/grit/chromeos_print_management_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/resources/grit/webui_resources.h"

namespace chromeos {
namespace printing {
namespace printing_manager {

PrintManagementUI::PrintManagementUI(
    content::WebUI* web_ui,
    BindPrintingMetadataProviderCallback callback)
    : ui::MojoWebUIController(web_ui),
      bind_pending_receiver_callback_(std::move(callback)) {
  auto html_source = base::WrapUnique(
      content::WebUIDataSource::Create(kChromeUIPrintManagementHost));
  html_source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources chrome://test 'self';");

  html_source->AddResourcePath("print_management.js", IDR_PRINT_MANAGEMENT_JS);
  html_source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER);
  html_source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER);
  html_source->AddResourcePath("printing_manager.mojom-lite.js",
                               IDR_PRINTING_MANAGER_MOJO_LITE_JS);
  html_source->AddResourcePath("mojo_interface_provider.js",
                               IDR_PRINT_MANAGEMENT_MOJO_INTERFACE_PROVIDER_JS);
  html_source->AddResourcePath("pwa.html", IDR_PRINT_MANAGEMENT_PWA_HTML);
  html_source->AddResourcePath("manifest.json", IDR_PRINT_MANAGEMENT_MANIFEST);
  html_source->AddResourcePath("app_icon_192.png", IDR_PRINT_MANAGEMENT_ICON);
  html_source->SetDefaultResource(IDR_PRINT_MANAGEMENT_INDEX_HTML);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source.release());
}

PrintManagementUI::~PrintManagementUI() = default;

void PrintManagementUI::BindInterface(
    mojo::PendingReceiver<mojom::PrintingMetadataProvider> receiver) {
  bind_pending_receiver_callback_.Run(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(PrintManagementUI)

}  // namespace printing_manager
}  // namespace printing
}  // namespace chromeos
