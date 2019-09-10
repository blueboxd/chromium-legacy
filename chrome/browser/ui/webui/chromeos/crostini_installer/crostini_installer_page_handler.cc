// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer_page_handler.h"

#include <utility>

#include "base/optional.h"

namespace chromeos {

CrostiniInstallerPageHandler::CrostiniInstallerPageHandler(
    mojo::PendingReceiver<chromeos::crostini_installer::mojom::PageHandler>
        pending_page_handler,
    mojo::PendingRemote<chromeos::crostini_installer::mojom::Page> pending_page)
    : receiver_{this, std::move(pending_page_handler)},
      page_{std::move(pending_page)} {}

CrostiniInstallerPageHandler::~CrostiniInstallerPageHandler() = default;

}  // namespace chromeos
