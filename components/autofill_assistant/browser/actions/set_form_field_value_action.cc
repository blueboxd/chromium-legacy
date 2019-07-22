// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/set_form_field_value_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/string_conversions_util.h"

namespace autofill_assistant {

SetFormFieldValueAction::SetFormFieldValueAction(ActionDelegate* delegate,
                                                 const ActionProto& proto)
    : Action(delegate, proto), weak_ptr_factory_(this) {
  DCHECK(proto_.has_set_form_value());
  DCHECK_GT(proto_.set_form_value().element().selectors_size(), 0);
  DCHECK_GT(proto_.set_form_value().value_size(), 0);
  DCHECK(proto_.set_form_value().value(0).has_text());
}

SetFormFieldValueAction::~SetFormFieldValueAction() {}

void SetFormFieldValueAction::InternalProcessAction(
    ProcessActionCallback callback) {
  Selector selector =
      Selector(proto_.set_form_value().element()).MustBeVisible();
  if (selector.empty()) {
    DVLOG(1) << __func__ << ": empty selector";
    UpdateProcessedAction(INVALID_SELECTOR);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }
  delegate_->ShortWaitForElement(
      selector, base::BindOnce(&SetFormFieldValueAction::OnWaitForElement,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(callback), selector));
}

void SetFormFieldValueAction::OnWaitForElement(ProcessActionCallback callback,
                                               const Selector& selector,
                                               bool element_found) {
  if (!element_found) {
    UpdateProcessedAction(ELEMENT_RESOLUTION_FAILED);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }
  // Start with first value, then call OnSetFieldValue() recursively until done.
  OnSetFieldValue(std::move(callback), selector, /* next = */ 0,
                  OkClientStatus());
}

void SetFormFieldValueAction::OnSetFieldValue(ProcessActionCallback callback,
                                              const Selector& selector,
                                              int next,
                                              const ClientStatus& status) {
  // If something went wrong or we are out of values: finish
  if (!status.ok() || next >= proto_.set_form_value().value_size()) {
    UpdateProcessedAction(status);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  const auto& key_field = proto_.set_form_value().value(next);
  bool simulate_key_presses = proto_.set_form_value().simulate_key_presses();
  int delay_in_millisecond = proto_.set_form_value().delay_in_millisecond();
  switch (key_field.keypress_case()) {
    case SetFormFieldValueProto_KeyPress::kText:
      if (simulate_key_presses || key_field.text().empty()) {
        // If we are already using keyboard simulation or we are trying to set
        // an empty value, no need to trigger keyboard fallback. Simply move on
        // to next value after |SetFieldValue| is done.
        delegate_->SetFieldValue(
            selector, key_field.text(), simulate_key_presses,
            delay_in_millisecond,
            base::BindOnce(&SetFormFieldValueAction::OnSetFieldValue,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                           selector,
                           /* next = */ next + 1));
      } else {
        // Trigger a check for keyboard fallback when |SetFieldValue| is done.
        delegate_->SetFieldValue(
            selector, key_field.text(), simulate_key_presses,
            delay_in_millisecond,
            base::BindOnce(
                &SetFormFieldValueAction::OnSetFieldValueAndCheckFallback,
                weak_ptr_factory_.GetWeakPtr(), std::move(callback), selector,
                /* next = */ next));
      }
      break;
    case SetFormFieldValueProto_KeyPress::kKeycode:
      // DEPRECATED: the field `keycode' used to contain a single character to
      // input as text. Since there is no easy way to convert keycodes to text,
      // this field is now deprecated and only works for US-ASCII characters.
      // You should use the `keyboard_input' field instead.
      if (key_field.keycode() < 128) {  // US-ASCII
        delegate_->SendKeyboardInput(
            selector, {key_field.keycode()}, delay_in_millisecond,
            base::BindOnce(&SetFormFieldValueAction::OnSetFieldValue,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                           selector,
                           /* next = */ next + 1));
      } else {
        DVLOG(3)
            << "SetFormFieldValueProto_KeyPress: field `keycode' is deprecated "
            << "and only supports US-ASCII values (encountered "
            << key_field.keycode() << "). Use field `key' instead.";
        OnSetFieldValue(std::move(callback), selector, next,
                        ClientStatus(INVALID_ACTION));
      }
      break;
    case SetFormFieldValueProto_KeyPress::kKeyboardInput:
      delegate_->SendKeyboardInput(
          selector, UTF8ToUnicode(key_field.keyboard_input()),
          delay_in_millisecond,
          base::BindOnce(&SetFormFieldValueAction::OnSetFieldValue,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         selector,
                         /* next = */ next + 1));
      break;
    default:
      DVLOG(1) << "Unrecognized field for SetFormFieldValueProto_KeyPress";
      OnSetFieldValue(std::move(callback), selector, next,
                      ClientStatus(INVALID_ACTION));
      break;
  }
}

void SetFormFieldValueAction::OnSetFieldValueAndCheckFallback(
    ProcessActionCallback callback,
    const Selector& selector,
    int next,
    const ClientStatus& status) {
  if (!status.ok()) {
    OnSetFieldValue(std::move(callback), selector, next + 1, status);
    return;
  }
  delegate_->GetFieldValue(
      selector, base::BindOnce(&SetFormFieldValueAction::OnGetFieldValue,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(callback), selector, next));
}

void SetFormFieldValueAction::OnGetFieldValue(ProcessActionCallback callback,
                                              const Selector& selector,
                                              int next,
                                              bool get_value_status,
                                              const std::string& value) {
  const auto& key_field = proto_.set_form_value().value(next);

  // Move to next value if |GetFieldValue| failed.
  if (!get_value_status) {
    OnSetFieldValue(std::move(callback), selector, next + 1, OkClientStatus());
    return;
  }

  // If value is still empty while it is not supposed to be, trigger keyboard
  // simulation fallback.
  if (key_field.text().size() > 0 && value.empty()) {
    // Report a key press simulation fallback has happened.
    auto result = SetFormFieldValueProto::Result();
    result.set_fallback_to_simulate_key_presses(true);
    *processed_action_proto_->mutable_set_form_field_value_result() = result;

    // Run |SetFieldValue| with keyboard simulation on and move on to next value
    // afterwards.
    delegate_->SetFieldValue(
        selector, key_field.text(), /*simulate_key_presses = */ true,
        proto_.set_form_value().delay_in_millisecond(),
        base::BindOnce(&SetFormFieldValueAction::OnSetFieldValue,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       selector,
                       /* next = */ next + 1));
    return;
  }

  // Move to next value in all other cases.
  OnSetFieldValue(std::move(callback), selector, next + 1, OkClientStatus());
}

}  // namespace autofill_assistant
