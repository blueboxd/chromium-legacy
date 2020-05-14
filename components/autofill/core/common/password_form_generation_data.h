// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_GENERATION_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_GENERATION_DATA_H_

#include <stdint.h>

#include "base/optional.h"
#include "build/build_config.h"
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
  PasswordFormGenerationData(FormRendererId form_renderer_id,
                             base::string16 new_password_element,
                             FieldRendererId new_password_renderer_id,
                             FieldRendererId confirmation_password_renderer_id);

  PasswordFormGenerationData(const PasswordFormGenerationData&);
  PasswordFormGenerationData& operator=(const PasswordFormGenerationData&);
  PasswordFormGenerationData(PasswordFormGenerationData&&);
  PasswordFormGenerationData& operator=(PasswordFormGenerationData&&);
  ~PasswordFormGenerationData();

  FormRendererId form_renderer_id;
  // TODO(crbug.com/1075444): Remove this once VotesUploader starts to use
  // unique renderer IDs.
  base::string16 new_password_element;
#endif
  FieldRendererId new_password_renderer_id;
  FieldRendererId confirmation_password_renderer_id;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_GENERATION_DATA_H_
