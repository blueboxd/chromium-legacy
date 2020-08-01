// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/promo_browser_command/promo_browser_command_handler.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/command_updater_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/promos/promo_service.h"
#include "chrome/browser/ui/browser.h"
#include "ui/base/window_open_disposition.h"

using promo_browser_command::mojom::ClickInfoPtr;
using promo_browser_command::mojom::Command;
using promo_browser_command::mojom::CommandHandler;

// static
const char PromoBrowserCommandHandler::kPromoBrowserCommandHistogramName[] =
    "NewTabPage.Promos.PromoBrowserCommand";

PromoBrowserCommandHandler::PromoBrowserCommandHandler(
    mojo::PendingReceiver<CommandHandler> pending_page_handler,
    Profile* profile)
    : profile_(profile),
      command_updater_(std::make_unique<CommandUpdaterImpl>(this)),
      page_handler_(this, std::move(pending_page_handler)) {
  // Explicitly enable supported commands.
  command_updater_->UpdateCommandEnabled(
      static_cast<int>(Command::kUnknownCommand), true);
}

PromoBrowserCommandHandler::~PromoBrowserCommandHandler() = default;

void PromoBrowserCommandHandler::ExecuteCommand(
    Command command_id,
    ClickInfoPtr click_info,
    ExecuteCommandCallback callback) {
  const auto disposition = ui::DispositionFromClick(
      click_info->middle_button, click_info->alt_key, click_info->ctrl_key,
      click_info->meta_key, click_info->shift_key);
  const bool command_executed = command_updater_->ExecuteCommandWithDisposition(
      static_cast<int>(command_id), disposition);
  std::move(callback).Run(command_executed);
}

void PromoBrowserCommandHandler::ExecuteCommandWithDisposition(
    int id,
    WindowOpenDisposition disposition) {
  const auto command = static_cast<Command>(id);
  base::UmaHistogramEnumeration(kPromoBrowserCommandHistogramName, command);

  switch (command) {
    case Command::kUnknownCommand:
      // Nothing to do.
      break;
    default:
      NOTREACHED() << "Unspecified behavior for command " << id;
      break;
  }
}

void PromoBrowserCommandHandler::SetCommandUpdaterForTesting(
    std::unique_ptr<CommandUpdater> command_updater) {
  command_updater_ = std::move(command_updater);
}
