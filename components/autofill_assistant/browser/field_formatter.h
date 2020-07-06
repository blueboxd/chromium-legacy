// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FIELD_FORMATTER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FIELD_FORMATTER_H_

#include <map>
#include <string>
#include "base/optional.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

namespace autofill_assistant {
namespace field_formatter {

// Replaces all placeholder occurrences of the form ${key} in |input| with the
// corresponding value in |mappings|, where |key| is an arbitrary string that
// does not contain curly braces. If |strict| is true, this will fail if any of
// the found placeholders is not in |mappings|. Otherwise, placeholders other
// than those from |mappings| will be left unchanged.
base::Optional<std::string> FormatString(
    const std::string& input,
    const std::map<std::string, std::string>& mappings,
    bool strict = true);

// Creates a lookup map for all non-empty autofill and custom
// AutofillFormatProto::AutofillAssistantCustomField field types in
// |autofill_data_model|.
// |locale| should be a locale string such as "en-US".
template <typename T>
std::map<std::string, std::string> CreateAutofillMappings(
    const T& autofill_data_model,
    const std::string& locale);

}  // namespace field_formatter
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FIELD_FORMATTER_H_
