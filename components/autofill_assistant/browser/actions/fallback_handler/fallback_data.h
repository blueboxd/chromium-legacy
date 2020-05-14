// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_FALLBACK_HANDLER_FALLBACK_DATA_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_FALLBACK_HANDLER_FALLBACK_DATA_H_

#include <map>

#include "base/optional.h"
#include "components/autofill/core/browser/data_model/form_group.h"

namespace autofill_assistant {

// Data necessary for filling in the fallback fields. This is kept in a
// separate struct to make sure we don't keep it for longer than strictly
// necessary.
struct FallbackData {
 public:
  FallbackData();
  ~FallbackData();

  FallbackData(const FallbackData&) = delete;
  FallbackData& operator=(const FallbackData&) = delete;

  // The key of the map. Should be either an entry of field_types.h or an
  // enum of Use*Action::AutofillAssistantCustomField.
  std::map<int, std::string> field_values;

  void AddFormGroup(const autofill::FormGroup& form_group);

  base::Optional<std::string> GetValueByExpression(
      const std::string& value_expression);

 private:
  base::Optional<std::string> GetValue(int key);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_FALLBACK_HANDLER_FALLBACK_DATA_H_
