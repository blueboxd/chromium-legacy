// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/password_form_generation_data.h"

#include <utility>

namespace autofill {

PasswordFormGenerationData::PasswordFormGenerationData() = default;

PasswordFormGenerationData::PasswordFormGenerationData(
    FieldRendererId new_password_renderer_id,
    FieldRendererId confirmation_password_renderer_id)
    : new_password_renderer_id(new_password_renderer_id),
      confirmation_password_renderer_id(confirmation_password_renderer_id) {}

#if defined(OS_IOS)
PasswordFormGenerationData::PasswordFormGenerationData(
    FormRendererId form_renderer_id,
    base::string16 new_password_element,
    FieldRendererId new_password_renderer_id,
    FieldRendererId confirmation_password_renderer_id)
    : form_renderer_id(form_renderer_id),
      new_password_element(new_password_element),
      new_password_renderer_id(new_password_renderer_id),
      confirmation_password_renderer_id(confirmation_password_renderer_id) {}

PasswordFormGenerationData::PasswordFormGenerationData(
    const PasswordFormGenerationData&) = default;

PasswordFormGenerationData& PasswordFormGenerationData::operator=(
    const PasswordFormGenerationData&) = default;

PasswordFormGenerationData::PasswordFormGenerationData(
    PasswordFormGenerationData&&) = default;

PasswordFormGenerationData& PasswordFormGenerationData::operator=(
    PasswordFormGenerationData&&) = default;

PasswordFormGenerationData::~PasswordFormGenerationData() = default;

#endif

}  // namespace autofill
