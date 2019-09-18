// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer_ui.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace chromeos {

bool CrostiniInstallerUI::IsEnabled() {
  return base::FeatureList::IsEnabled(
      chromeos::features::kCrostiniWebUIInstaller);
}

CrostiniInstallerUI::CrostiniInstallerUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI{web_ui} {
  // TODO(lxj): We might want to make sure there is only one instance of this
  // class.

  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUICrostiniInstallerHost);

  source->AddResourcePath("app.html", IDR_CROSTINI_INSTALLER_APP_HTML);
  source->AddResourcePath("app.js", IDR_CROSTINI_INSTALLER_APP_JS);
  source->AddResourcePath("browser_proxy.html",
                          IDR_CROSTINI_INSTALLER_BROWSER_PROXY_HTML);
  source->AddResourcePath("browser_proxy.js",
                          IDR_CROSTINI_INSTALLER_BROWSER_PROXY_JS);
  source->AddResourcePath("crostini_installer.mojom.html",
                          IDR_CROSTINI_INSTALLER_MOJO_HTML);
  source->AddResourcePath("crostini_installer.mojom-lite.js",
                          IDR_CROSTINI_INSTALLER_MOJO_LITE_JS);
  source->SetDefaultResource(IDR_CROSTINI_INSTALLER_INDEX_HTML);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);

  AddHandlerToRegistry(base::BindRepeating(
      &CrostiniInstallerUI::BindPageHandlerFactory, base::Unretained(this)));
}

CrostiniInstallerUI::~CrostiniInstallerUI() = default;

void CrostiniInstallerUI::BindPageHandlerFactory(
    mojo::PendingReceiver<
        chromeos::crostini_installer::mojom::PageHandlerFactory>
        pending_receiver) {
  if (page_factory_receiver_.is_bound()) {
    page_factory_receiver_.reset();
  }

  page_factory_receiver_.Bind(std::move(pending_receiver));
}

void CrostiniInstallerUI::CreatePageHandler(
    mojo::PendingRemote<chromeos::crostini_installer::mojom::Page> pending_page,
    mojo::PendingReceiver<chromeos::crostini_installer::mojom::PageHandler>
        pending_page_handler) {
  DCHECK(pending_page.is_valid());

  page_handler_ = std::make_unique<CrostiniInstallerPageHandler>(
      std::move(pending_page_handler), std::move(pending_page));
}

}  // namespace chromeos
