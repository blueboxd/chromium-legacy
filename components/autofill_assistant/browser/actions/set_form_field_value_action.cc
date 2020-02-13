// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/set_form_field_value_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures_util.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {
namespace {

bool IsSimulatingKeyPresses(KeyboardValueFillStrategy fill_strategy) {
  return fill_strategy == SIMULATE_KEY_PRESSES ||
         fill_strategy == SIMULATE_KEY_PRESSES_SELECT_VALUE;
}

}  // namespace

SetFormFieldValueAction::FieldInput::FieldInput(
    std::unique_ptr<std::vector<UChar32>> _keyboard_input)
    : keyboard_input(std::move(_keyboard_input)) {}

SetFormFieldValueAction::FieldInput::FieldInput(std::string _value)
    : value(_value) {}

SetFormFieldValueAction::FieldInput::FieldInput(
    PasswordValueType _password_type,
    const std::string& _memory_key)
    : password_type(_password_type), memory_key(_memory_key) {
  DCHECK_EQ(password_type == PasswordValueType::GENERATED_PASSWORD,
            !memory_key.empty())
      << "Wrong |FieldInput| initialisation: iff the password action type is "
         "|GENERATED_PASSWORD|, the memory should not be empty";
}

SetFormFieldValueAction::FieldInput::FieldInput(FieldInput&& other) = default;

SetFormFieldValueAction::FieldInput::~FieldInput() {}

SetFormFieldValueAction::SetFormFieldValueAction(ActionDelegate* delegate,
                                                 const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_set_form_value());
  DCHECK_GT(proto_.set_form_value().element().selectors_size(), 0);
  DCHECK_GT(proto_.set_form_value().value_size(), 0);
}

SetFormFieldValueAction::~SetFormFieldValueAction() {}

void SetFormFieldValueAction::InternalProcessAction(
    ProcessActionCallback callback) {
  process_action_callback_ = std::move(callback);
  selector_ = Selector(proto_.set_form_value().element()).MustBeVisible();
  if (selector_.empty()) {
    VLOG(1) << __func__ << ": empty selector";
    EndAction(ClientStatus(INVALID_SELECTOR));
    return;
  }

  // Check proto fields.
  for (const auto& keypress : proto_.set_form_value().value()) {
    switch (keypress.keypress_case()) {
      case SetFormFieldValueProto_KeyPress::kKeycode:
        // DEPRECATED: the field `keycode' used to contain a single character to
        // input as text. Since there is no easy way to convert keycodes to
        // text, this field is now deprecated and only works for US-ASCII
        // characters. You should use the `keyboard_input' field instead.
        if (keypress.keycode() >= 128) {
          VLOG(1) << "SetFormFieldValueAction: field `keycode' is deprecated "
                  << "and only supports US-ASCII values (encountered value > "
                     "127). Use field `key' instead.";
          EndAction(ClientStatus(INVALID_ACTION));
          return;
        }
        field_inputs_.emplace_back(
            /* keyboard_input = */ std::make_unique<std::vector<UChar32>>(
                1, keypress.keycode()));
        break;
      case SetFormFieldValueProto_KeyPress::kKeyboardInput:
        if (keypress.keyboard_input().empty()) {
          VLOG(1) << "SetFormFieldValueAction: field 'keyboard_input' must be "
                     "non-empty if set.";
          EndAction(ClientStatus(INVALID_ACTION));
          return;
        }
        field_inputs_.emplace_back(
            /* keyboard_input = */ std::make_unique<std::vector<UChar32>>(
                UTF8ToUnicode(keypress.keyboard_input())));
        break;
      case SetFormFieldValueProto_KeyPress::kUseUsername:
        FALLTHROUGH;
      case SetFormFieldValueProto_KeyPress::kUsePassword:
        // Login information must have been stored by a previous action.
        if (!delegate_->GetUserData()->selected_login_.has_value()) {
          VLOG(1) << "SetFormFieldValueAction: requested login details not "
                     "available in client memory.";
          EndAction(ClientStatus(PRECONDITION_FAILED));
          return;
        }
        if (keypress.keypress_case() ==
            SetFormFieldValueProto_KeyPress::kUseUsername) {
          field_inputs_.emplace_back(/* value = */ delegate_->GetUserData()
                                         ->selected_login_->username);
        } else {
          field_inputs_.emplace_back(
              /* password_type = */ PasswordValueType::STORED_PASSWORD);
        }
        break;
      case SetFormFieldValueProto_KeyPress::kText:
        // Currently no check required.
        field_inputs_.emplace_back(/* value = */ keypress.text());
        break;
      case SetFormFieldValueProto_KeyPress::kClientMemoryKey:
        if (keypress.client_memory_key().empty()) {
          VLOG(1) << "SetFormFieldValueAction: empty |client_memory_key|";
          EndAction(ClientStatus(INVALID_ACTION));
          return;
        }
        if (!delegate_->GetUserData()->has_additional_value(
                keypress.client_memory_key()) ||
            delegate_->GetUserData()
                    ->additional_value(keypress.client_memory_key())
                    ->strings()
                    .values()
                    .size() != 1) {
          VLOG(1) << "SetFormFieldValueAction: requested key '"
                  << keypress.client_memory_key()
                  << "' not available in client memory";
          EndAction(ClientStatus(PRECONDITION_FAILED));
          return;
        }
        field_inputs_.emplace_back(
            /* value = */ delegate_->GetUserData()
                ->additional_value(keypress.client_memory_key())
                ->strings()
                .values(0));
        break;
      case SetFormFieldValueProto_KeyPress::kGeneratePassword:
        if (keypress.generate_password().memory_key().empty()) {
          VLOG(1) << "SetFormFieldValueAction_kGeneratePassword: "
                     "|memory_key| cannot be empty.";
          EndAction(ClientStatus(INVALID_ACTION));
          return;
        }
        field_inputs_.emplace_back(
            /* password_type = */ PasswordValueType::GENERATED_PASSWORD,
            /* memory_key = */ keypress.generate_password().memory_key());
        break;
      default:
        VLOG(1) << "Unrecognized field for SetFormFieldValueProto_KeyPress";
        EndAction(ClientStatus(INVALID_ACTION));
        return;
    }
  }

  delegate_->ShortWaitForElement(
      selector_, base::BindOnce(&SetFormFieldValueAction::OnWaitForElement,
                                weak_ptr_factory_.GetWeakPtr()));
}

void SetFormFieldValueAction::OnWaitForElement(
    const ClientStatus& element_status) {
  if (!element_status.ok()) {
    EndAction(ClientStatus(element_status.proto_status()));
    return;
  }
  // Start with first value, then call OnSetFieldValue() recursively until done.
  OnSetFieldValue(/* next = */ 0, OkClientStatus());
}

void SetFormFieldValueAction::OnSetFieldValue(int next,
                                              const ClientStatus& status) {
  // If something went wrong or we are out of values: finish
  if (!status.ok() || next >= proto_.set_form_value().value_size()) {
    EndAction(status);
    return;
  }

  int delay_in_millisecond = proto_.set_form_value().delay_in_millisecond();
  auto next_field_callback = base::BindOnce(
      &SetFormFieldValueAction::OnSetFieldValue, weak_ptr_factory_.GetWeakPtr(),
      /* next = */ next + 1);
  const auto& field_input = field_inputs_[next];
  if (field_input.keyboard_input) {
    delegate_->SendKeyboardInput(selector_, *field_input.keyboard_input,
                                 delay_in_millisecond,
                                 std::move(next_field_callback));
  } else if (field_input.password_type != PasswordValueType::NOT_SET) {
    switch (field_input.password_type) {
      case PasswordValueType::NOT_SET:
        DCHECK(false);
        break;
      case PasswordValueType::GENERATED_PASSWORD:
        delegate_->RetrieveElementFormAndFieldData(
            selector_,
            base::BindOnce(
                &SetFormFieldValueAction::OnGetFormAndFieldDataForGeneration,
                weak_ptr_factory_.GetWeakPtr(),
                /* field_index = */ next, field_input.memory_key));
        break;
      case PasswordValueType::STORED_PASSWORD:
        delegate_->GetWebsiteLoginFetcher()->GetPasswordForLogin(
            *delegate_->GetUserData()->selected_login_,
            base::BindOnce(&SetFormFieldValueAction::OnGetStoredPassword,
                           weak_ptr_factory_.GetWeakPtr(),
                           /* field_index = */ next));
        break;
    }
  } else {
    auto fill_strategy = proto_.set_form_value().fill_strategy();
    if (IsSimulatingKeyPresses(fill_strategy)) {
      delegate_->SetFieldValue(selector_, field_input.value, fill_strategy,
                               delay_in_millisecond,
                               std::move(next_field_callback));
    } else {
      delegate_->SetFieldValue(
          selector_, field_input.value, fill_strategy, delay_in_millisecond,
          base::BindOnce(
              &SetFormFieldValueAction::OnSetFieldValueAndCheckFallback,
              weak_ptr_factory_.GetWeakPtr(),
              /* field_index = */ next,
              /* requested_value = */ field_input.value));
    }
  }
}

void SetFormFieldValueAction::OnSetFieldValueAndCheckFallback(
    int field_index,
    const std::string& requested_value,
    const ClientStatus& status) {
  if (!status.ok()) {
    EndAction(status);
    return;
  }
  delegate_->GetFieldValue(
      selector_, base::BindOnce(&SetFormFieldValueAction::OnGetFieldValue,
                                weak_ptr_factory_.GetWeakPtr(), field_index,
                                requested_value));
}

void SetFormFieldValueAction::OnGetFieldValue(
    int field_index,
    const std::string& requested_value,
    const ClientStatus& element_status,
    const std::string& actual_value) {
  // Move to next value if |GetFieldValue| failed.
  if (!element_status.ok()) {
    OnSetFieldValue(field_index + 1, OkClientStatus());
    return;
  }

  // If value is still empty while it is not supposed to be, trigger keyboard
  // simulation fallback.
  if (!requested_value.empty() && actual_value.empty()) {
    // Report a key press simulation fallback has happened.
    auto result = SetFormFieldValueProto::Result();
    result.set_fallback_to_simulate_key_presses(true);
    *processed_action_proto_->mutable_set_form_field_value_result() = result;

    // Run |SetFieldValue| with keyboard simulation on and move on to next value
    // afterwards.
    delegate_->SetFieldValue(
        selector_, requested_value, SIMULATE_KEY_PRESSES,
        proto_.set_form_value().delay_in_millisecond(),
        base::BindOnce(&SetFormFieldValueAction::OnSetFieldValue,
                       weak_ptr_factory_.GetWeakPtr(),
                       /* next = */ field_index + 1));
    return;
  }

  // Move to next value in all other cases.
  OnSetFieldValue(field_index + 1, OkClientStatus());
}

void SetFormFieldValueAction::OnGetStoredPassword(int field_index,
                                                  bool success,
                                                  std::string password) {
  if (!success) {
    EndAction(ClientStatus(AUTOFILL_INFO_NOT_AVAILABLE));
    return;
  }
  auto fill_strategy = proto_.set_form_value().fill_strategy();
  int delay_in_millisecond = proto_.set_form_value().delay_in_millisecond();
  if (IsSimulatingKeyPresses(fill_strategy)) {
    delegate_->SetFieldValue(
        selector_, password, fill_strategy, delay_in_millisecond,
        base::BindOnce(&SetFormFieldValueAction::OnSetFieldValue,
                       weak_ptr_factory_.GetWeakPtr(),
                       /* next = */ field_index + 1));
  } else {
    delegate_->SetFieldValue(
        selector_, password, fill_strategy, delay_in_millisecond,
        base::BindOnce(
            &SetFormFieldValueAction::OnSetFieldValueAndCheckFallback,
            weak_ptr_factory_.GetWeakPtr(),
            /* next = */ field_index, /* requested_value = */ password));
  }
}

void SetFormFieldValueAction::OnGetFormAndFieldDataForGeneration(
    int field_index,
    const std::string memory_key,
    const ClientStatus& status,
    const autofill::FormData& form_data,
    const autofill::FormFieldData& field_data) {
  if (!status.ok()) {
    EndAction(status);
  }
  uint64_t max_length = field_data.max_length;
  std::string password = delegate_->GetWebsiteLoginFetcher()->GeneratePassword(
      autofill::CalculateFormSignature(form_data),
      autofill::CalculateFieldSignatureForField(field_data), max_length);
  delegate_->WriteUserData(
      base::BindOnce(&SetFormFieldValueAction::StoreGeneratedPasswordToUserData,
                     weak_ptr_factory_.GetWeakPtr(), memory_key, password));

  auto fill_strategy = proto_.set_form_value().fill_strategy();
  int delay_in_millisecond = proto_.set_form_value().delay_in_millisecond();
  if (IsSimulatingKeyPresses(fill_strategy)) {
    delegate_->SetFieldValue(
        selector_, password, fill_strategy, delay_in_millisecond,
        base::BindOnce(&SetFormFieldValueAction::OnSetFieldValue,
                       weak_ptr_factory_.GetWeakPtr(),
                       /* next = */ field_index + 1));
  } else {
    delegate_->SetFieldValue(
        selector_, password, fill_strategy, delay_in_millisecond,
        base::BindOnce(
            &SetFormFieldValueAction::OnSetFieldValueAndCheckFallback,
            weak_ptr_factory_.GetWeakPtr(),
            /* next = */ field_index, /* requested_value = */ password));
  }
}

void SetFormFieldValueAction::StoreGeneratedPasswordToUserData(
    const std::string memory_key,
    const std::string generated_password,
    UserData* user_data,
    UserData::FieldChange* field_change) {
  DCHECK(user_data);
  ValueProto value;
  value.mutable_strings()->add_values(generated_password);
  user_data->additional_values_[memory_key] = value;
}

void SetFormFieldValueAction::EndAction(const ClientStatus& status) {
  // Clear immediately, to prevent sensitive information from staying in memory.
  field_inputs_.clear();
  UpdateProcessedAction(status);
  std::move(process_action_callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
