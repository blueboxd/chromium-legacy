// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_GENERATION_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_GENERATION_DATA_H_

#include <stdint.h>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/renderer_id.h"
#include "url/gurl.h"

namespace autofill {

// Structure used for sending information from browser to renderer about on
// which fields password should be generated.
struct PasswordFormGenerationData {
  PasswordFormGenerationData();
  PasswordFormGenerationData(FieldRendererId new_password_renderer_id,
                             FieldRendererId confirmation_password_renderer_id);
#if defined(OS_IOS)
  PasswordFormGenerationData(base::string16 form_name,
                             base::string16 new_password_element,
                             base::string16 confirmation_password_element);

  PasswordFormGenerationData(const PasswordFormGenerationData&);
  PasswordFormGenerationData& operator=(const PasswordFormGenerationData&);
  PasswordFormGenerationData(PasswordFormGenerationData&&);
  PasswordFormGenerationData& operator=(PasswordFormGenerationData&&);
  ~PasswordFormGenerationData();

  base::string16 form_name;
  base::string16 new_password_element;
  base::string16 confirmation_password_element;
#endif
  FieldRendererId new_password_renderer_id;
  FieldRendererId confirmation_password_renderer_id;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_GENERATION_DATA_H_
