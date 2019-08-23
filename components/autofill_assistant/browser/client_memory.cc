// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/client_memory.h"

#include <algorithm>

#include "base/strings/string_util.h"

namespace autofill_assistant {

ClientMemory::ClientMemory() = default;
ClientMemory::~ClientMemory() = default;

const autofill::CreditCard* ClientMemory::selected_card() {
  if (selected_card_.has_value())
    return selected_card_->get();
  return nullptr;
}

bool ClientMemory::has_selected_card() {
  return selected_card_.has_value();
}

const autofill::AutofillProfile* ClientMemory::selected_address(
    const std::string& name) {
  if (selected_addresses_.find(name) != selected_addresses_.end())
    return selected_addresses_[name].get();

  return nullptr;
}

void ClientMemory::set_selected_card(
    std::unique_ptr<autofill::CreditCard> card) {
  selected_card_ = std::move(card);
}

void ClientMemory::set_selected_address(
    const std::string& name,
    std::unique_ptr<autofill::AutofillProfile> address) {
  selected_addresses_[name] = std::move(address);
}

bool ClientMemory::has_selected_address(const std::string& name) {
  return selected_addresses_.find(name) != selected_addresses_.end();
}

std::string ClientMemory::GetAllAddressKeyNames() const {
  std::vector<std::string> entries;
  for (const auto& entry : selected_addresses_) {
    entries.emplace_back(entry.first);
  }
  std::sort(entries.begin(), entries.end());
  return base::JoinString(entries, ",");
}

}  // namespace autofill_assistant
