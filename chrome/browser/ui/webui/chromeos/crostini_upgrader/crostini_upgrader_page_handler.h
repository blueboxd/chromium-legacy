// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_UPGRADER_CROSTINI_UPGRADER_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_UPGRADER_CROSTINI_UPGRADER_PAGE_HANDLER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/crostini/crostini_upgrader_ui_delegate.h"
#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

class CrostiniUpgraderPageHandler
    : public chromeos::crostini_upgrader::mojom::PageHandler,
      public crostini::CrostiniUpgraderUIObserver {
 public:
  CrostiniUpgraderPageHandler(
      crostini::CrostiniUpgraderUIDelegate* upgrader_ui_delegate,
      mojo::PendingReceiver<chromeos::crostini_upgrader::mojom::PageHandler>
          pending_page_handler,
      mojo::PendingRemote<chromeos::crostini_upgrader::mojom::Page>
          pending_page,
      base::OnceClosure close_dialog_callback,
      base::OnceClosure launch_closure);
  ~CrostiniUpgraderPageHandler() override;

  // chromeos::crostini_upgrader::mojom::PageHandler:
  void Backup() override;
  void Upgrade() override;
  void Cancel() override;
  void CancelBeforeStart() override;
  void Close() override;
  void Launch() override;

  // CrostiniUpgraderUIObserver
  void OnBackupProgress(int percent) override;
  void OnBackupSucceeded() override;
  void OnBackupFailed() override;
  void OnUpgradeProgress(const std::vector<std::string>& messages) override;
  void OnUpgradeSucceeded() override;
  void OnUpgradeFailed() override;
  void OnCanceled() override;

 private:
  // Not owned.
  crostini::CrostiniUpgraderUIDelegate* upgrader_ui_delegate_;
  mojo::Receiver<chromeos::crostini_upgrader::mojom::PageHandler> receiver_;
  mojo::Remote<chromeos::crostini_upgrader::mojom::Page> page_;
  base::OnceClosure close_dialog_callback_;
  base::OnceClosure launch_closure_;

  base::WeakPtrFactory<CrostiniUpgraderPageHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrostiniUpgraderPageHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_UPGRADER_CROSTINI_UPGRADER_PAGE_HANDLER_H_
