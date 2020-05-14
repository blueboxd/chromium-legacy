// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/fallback_handler/required_field.h"

namespace autofill_assistant {

RequiredField::RequiredField() = default;

RequiredField::~RequiredField() = default;

RequiredField::RequiredField(const RequiredField& copy) = default;

void RequiredField::FromProto(
    const UseAddressProto::RequiredField& required_field_proto) {
  value_expression = required_field_proto.value_expression();
  selector = Selector(required_field_proto.element());
  fill_strategy = required_field_proto.fill_strategy();
  select_strategy = required_field_proto.select_strategy();
  delay_in_millisecond = required_field_proto.delay_in_millisecond();
  if (required_field_proto.has_option_element_to_click()) {
    fallback_click_element =
        Selector(required_field_proto.option_element_to_click());
    click_type = required_field_proto.click_type();
  }
  forced = required_field_proto.forced();
}

void RequiredField::FromProto(
    const UseCreditCardProto::RequiredField& required_field_proto) {
  value_expression = required_field_proto.value_expression();
  selector = Selector(required_field_proto.element());
  fill_strategy = required_field_proto.fill_strategy();
  select_strategy = required_field_proto.select_strategy();
  delay_in_millisecond = required_field_proto.delay_in_millisecond();
  if (required_field_proto.has_option_element_to_click()) {
    fallback_click_element =
        Selector(required_field_proto.option_element_to_click());
    click_type = required_field_proto.click_type();
  }
  forced = required_field_proto.forced();
}

bool RequiredField::ShouldFallback(bool has_fallback_data) const {
  return (status == EMPTY && !fallback_click_element.has_value()) ||
         (forced && has_fallback_data) ||
         (fallback_click_element.has_value() && has_fallback_data);
}

}  // namespace autofill_assistant
