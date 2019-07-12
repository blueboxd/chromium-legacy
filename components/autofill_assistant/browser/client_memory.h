// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_MEMORY_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_MEMORY_H_

#include <map>
#include <string>

#include "base/optional.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

namespace autofill_assistant {
// Data shared between scripts and actions.
class ClientMemory {
 public:
  ClientMemory();
  virtual ~ClientMemory();

  // Selected credit card, if any. It will be a nullptr if didn't select
  // anything or if selected 'Fill manually'.
  virtual const autofill::CreditCard* selected_card();

  // Return true if card has been selected, otherwise return false.
  // Note that selected_card() might return nullptr when has_selected_card() is
  // true because fill manually was chosen.
  virtual bool has_selected_card();

  // Selected address for |name|. It will be a nullptr if didn't select anything
  // or if selected 'Fill manually'.
  virtual const autofill::AutofillProfile* selected_address(
      const std::string& name);

  // Return true if address has been selected, otherwise return false.
  // Note that selected_address() might return nullptr when
  // has_selected_address() is true because fill manually was chosen.
  virtual bool has_selected_address(const std::string& name);

  // Set the selected card.
  virtual void set_selected_card(std::unique_ptr<autofill::CreditCard> card);

  // Set the selected address for |name|.
  virtual void set_selected_address(
      const std::string& name,
      std::unique_ptr<autofill::AutofillProfile> address);

  virtual std::string GetAllAddressKeyNames() const;

 private:
  base::Optional<std::unique_ptr<autofill::CreditCard>> selected_card_;

  // The address key requested by the autofill action.
  std::map<std::string, std::unique_ptr<autofill::AutofillProfile>>
      selected_addresses_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_MEMORY_H_
