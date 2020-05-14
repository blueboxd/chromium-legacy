// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/fallback_handler/fallback_data.h"

#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/form_group.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/re2/src/re2/stringpiece.h"

namespace autofill_assistant {
namespace {

// Matches a single $ followed by an opening {, then an inner key and a closing
// }.
const char kKeyPattern[] = R"re(\$\{([^}]+)\})re";

}  // namespace

FallbackData::FallbackData() = default;

FallbackData::~FallbackData() = default;

void FallbackData::AddFormGroup(const autofill::FormGroup& form_group) {
  autofill::ServerFieldTypeSet available_fields;
  form_group.GetNonEmptyTypes("en-US", &available_fields);
  for (const auto& field : available_fields) {
    field_values.emplace(static_cast<int>(field),
                         base::UTF16ToUTF8(form_group.GetInfo(
                             autofill::AutofillType(field), "en-US")));
  }
}

base::Optional<std::string> FallbackData::GetValue(int key) {
  auto it = field_values.find(key);
  if (it != field_values.end() && !it->second.empty()) {
    return it->second;
  }
  return base::nullopt;
}

base::Optional<std::string> FallbackData::GetValueByExpression(
    const std::string& value_expression) {
  int key;
  if (base::StringToInt(value_expression, &key)) {
    return GetValue(key);
  }

  std::string out = value_expression;

  re2::StringPiece input(value_expression);
  while (re2::RE2::FindAndConsume(&input, kKeyPattern, &key)) {
    auto rewrite_value = GetValue(key);
    if (!rewrite_value.has_value()) {
      DVLOG(3) << "No value for " << key << " in " << value_expression;
      return base::nullopt;
    }

    re2::RE2::Replace(&out, kKeyPattern,
                      re2::StringPiece(rewrite_value.value()));
  }

  if (out.empty()) {
    return base::nullopt;
  }
  return out;
}

}  // namespace autofill_assistant
