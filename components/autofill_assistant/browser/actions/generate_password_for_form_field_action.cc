// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/generate_password_for_form_field_action.h"

#include <utility>
#include "base/optional.h"

#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/value_util.h"

namespace autofill_assistant {

GeneratePasswordForFormFieldAction::GeneratePasswordForFormFieldAction(
    ActionDelegate* delegate,
    const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_generate_password_for_form_field());
}

GeneratePasswordForFormFieldAction::~GeneratePasswordForFormFieldAction() {}

void GeneratePasswordForFormFieldAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);

  selector_ = Selector(proto_.generate_password_for_form_field().element())
                  .MustBeVisible();

  if (selector_.empty()) {
    VLOG(1) << __func__ << ": empty selector";
    EndAction(ClientStatus(INVALID_SELECTOR));
    return;
  }

  delegate_->RetrieveElementFormAndFieldData(
      selector_,
      base::BindOnce(&GeneratePasswordForFormFieldAction::
                         OnGetFormAndFieldDataForGeneration,
                     weak_ptr_factory_.GetWeakPtr(),
                     proto_.generate_password_for_form_field().memory_key()));
}

void GeneratePasswordForFormFieldAction::OnGetFormAndFieldDataForGeneration(
    const std::string& memory_key,
    const ClientStatus& status,
    const autofill::FormData& form_data,
    const autofill::FormFieldData& field_data) {
  if (!status.ok()) {
    EndAction(status);
  }

  if (!delegate_->GetUserData()->selected_login_.has_value()) {
    VLOG(1) << "GeneratePasswordForFormFieldAction: requested login details "
               "not available in client memory.";
    EndAction(ClientStatus(PRECONDITION_FAILED));
    return;
  }

  uint64_t max_length = field_data.max_length;
  std::string password = delegate_->GetWebsiteLoginManager()->GeneratePassword(
      autofill::CalculateFormSignature(form_data),
      autofill::CalculateFieldSignatureForField(field_data), max_length);

  delegate_->WriteUserData(base::BindOnce(
      &GeneratePasswordForFormFieldAction::StoreGeneratedPasswordToUserData,
      weak_ptr_factory_.GetWeakPtr(), memory_key, password));

  // Presaving stores a generated password with empty username for the cases
  // when Chrome misses or misclassifies a successful submission. Thus, even if
  // a site saves/updates the password and Chrome doesn't, the generated
  // password will be in the password store.
  // Ideally, a generated password should be presaved after form filling.
  // Otherwise, if filling fails and submission cannot happen for sure, the
  // presaved password is pointless.
  delegate_->GetWebsiteLoginManager()->PresaveGeneratedPassword(
      *delegate_->GetUserData()->selected_login_, password, form_data,
      base::BindOnce(&GeneratePasswordForFormFieldAction::EndAction,
                     weak_ptr_factory_.GetWeakPtr(),
                     ClientStatus(ACTION_APPLIED)));
}

void GeneratePasswordForFormFieldAction::StoreGeneratedPasswordToUserData(
    const std::string& memory_key,
    const std::string& generated_password,
    UserData* user_data,
    UserData::FieldChange* field_change) {
  DCHECK(user_data);
  user_data->additional_values_[memory_key] = SimpleValue(generated_password);
}

void GeneratePasswordForFormFieldAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
