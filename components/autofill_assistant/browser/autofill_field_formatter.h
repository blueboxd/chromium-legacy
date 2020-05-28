// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_AUTOFILL_FIELD_FORMATTER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_AUTOFILL_FIELD_FORMATTER_H_

#include <map>
#include <string>
#include "base/optional.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

namespace autofill_assistant {
namespace autofill_field_formatter {

// Replaces all placeholder occurrences of the form ${N} in |pattern| with the
// corresponding value in |mappings|. Returns the result or nullopt if any
// of the requested fields was not available. As a special case, input patterns
// containing a single integer are also allowed and implicitly interpreted as
// ${N}.
base::Optional<std::string> FormatString(
    const std::string& pattern,
    const std::map<int, std::string> mappings);

// Creates a lookup map for all non-empty autofill and custom
// AutofillFormatProto::AutofillAssistantCustomField field types in
// |autofill_data_model|.
// |locale| should be a locale string such as "en-US".
template <typename T>
std::map<int, std::string> CreateAutofillMappings(const T& autofill_data_model,
                                                  const std::string& locale);

}  // namespace autofill_field_formatter
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_AUTOFILL_FIELD_FORMATTER_H_
