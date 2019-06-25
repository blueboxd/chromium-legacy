// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PASSWORD_FORM_FIELD_PREDICTION_MAP_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PASSWORD_FORM_FIELD_PREDICTION_MAP_H_

#include "base/containers/flat_map.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"

namespace autofill {

using PasswordFormFieldPredictionMap =
    base::flat_map<FormFieldData, mojom::PasswordFormFieldPredictionType>;
using FormsPredictionsMap =
    base::flat_map<FormData, PasswordFormFieldPredictionMap>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PASSWORD_FORM_FIELD_PREDICTION_MAP_H_
