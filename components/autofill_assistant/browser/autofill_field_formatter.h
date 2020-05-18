// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_AUTOFILL_FIELD_FORMATTER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_AUTOFILL_FIELD_FORMATTER_H_

#include <string>
#include "base/optional.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

namespace autofill_assistant {
namespace autofill_field_formatter {

// Replaces all placeholder occurrences of the form ${N} in |pattern| with the
// corresponding autofill field values. Returns the result or nullopt if any
// of the requested fields was not available. As a special case, input patterns
// containing a single integer are also allowed and implicitly interpreted as
// ${N}.
// |locale| should be a locale string such as "en-US".
//
// Some autofill models may support additional field values as specified in
// AutofillFormatProto::AutofillAssistantCustomField.
template <class T>
base::Optional<std::string> FormatString(const T& autofill_data_model,
                                         const std::string& pattern,
                                         const std::string& locale);

}  // namespace autofill_field_formatter
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_AUTOFILL_FIELD_FORMATTER_H_
