// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_INSTALLER_CROSTINI_INSTALLER_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_INSTALLER_CROSTINI_INSTALLER_PAGE_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

class CrostiniInstallerPageHandler
    : public chromeos::crostini_installer::mojom::PageHandler {
 public:
  CrostiniInstallerPageHandler(
      mojo::PendingReceiver<chromeos::crostini_installer::mojom::PageHandler>
          pending_page_handler,
      mojo::PendingRemote<chromeos::crostini_installer::mojom::Page>
          pending_page);
  ~CrostiniInstallerPageHandler() override;

 private:
  mojo::Receiver<chromeos::crostini_installer::mojom::PageHandler> receiver_;
  mojo::Remote<chromeos::crostini_installer::mojom::Page> page_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniInstallerPageHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_INSTALLER_CROSTINI_INSTALLER_PAGE_HANDLER_H_
