// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler_registry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"

namespace send_tab_to_self {

SendTabToSelfClientService::SendTabToSelfClientService(
    Profile* profile,
    SendTabToSelfModel* model) {
  model_ = model;
  model_->AddObserver(this);

  SetupHandlerRegistry(profile);
}

SendTabToSelfClientService::~SendTabToSelfClientService() {
  model_->RemoveObserver(this);
  model_ = nullptr;
}

void SendTabToSelfClientService::SendTabToSelfModelLoaded() {
  // TODO(crbug.com/949756): Push changes that happened before the model was
  // loaded.
}

void SendTabToSelfClientService::EntriesAddedRemotely(
    const std::vector<const SendTabToSelfEntry*>& new_entries) {
  for (const std::unique_ptr<ReceivingUiHandler>& handler : GetHandlers()) {
    handler->DisplayNewEntries(new_entries);
  }
}

void SendTabToSelfClientService::EntriesRemovedRemotely(
    const std::vector<std::string>& guids) {
  for (const std::unique_ptr<ReceivingUiHandler>& handler : GetHandlers()) {
    handler->DismissEntries(guids);
  }
}

void SendTabToSelfClientService::SetupHandlerRegistry(Profile* profile) {
  registry_ = ReceivingUiHandlerRegistry::GetInstance();
  registry_->InstantiatePlatformSpecificHandlers(profile);
}

const std::vector<std::unique_ptr<ReceivingUiHandler>>&
SendTabToSelfClientService::GetHandlers() const {
  return registry_->GetHandlers();
}

}  // namespace send_tab_to_self
