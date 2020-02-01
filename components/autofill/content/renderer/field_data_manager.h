// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_FIELD_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_FIELD_DATA_MANAGER_H_

#include <map>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "components/autofill/core/common/form_field_data.h"

namespace blink {
class WebFormControlElement;
}

namespace autofill {

// This class provides the meathods to update and get the field data (the pair
// of user typed value and field properties mask).
class FieldDataManager {
 public:
  FieldDataManager();
  ~FieldDataManager();

  void ClearData();
  bool HasFieldData(uint32_t id) const;

  // Updates the field value associated with the key |element| in
  // |field_value_and_properties_map_|.
  // Flags in |mask| are added with bitwise OR operation.
  // If |value| is empty, USER_TYPED and AUTOFILLED should be cleared.
  void UpdateFieldDataMap(const blink::WebFormControlElement& element,
                          const base::string16& value,
                          FieldPropertiesMask mask);
  // Only update FieldPropertiesMask when value is null.
  void UpdateFieldDataMapWithNullValue(
      const blink::WebFormControlElement& element,
      FieldPropertiesMask mask);

  base::string16 GetUserTypedValue(uint32_t id) const;
  FieldPropertiesMask GetFieldPropertiesMask(uint32_t id) const;

  // Check if the string |value| is saved in |field_value_and_properties_map_|.
  bool FindMachedValue(const base::string16& value) const;

  bool DidUserType(uint32_t id) const;

 private:
  std::map<uint32_t,
           std::pair<base::Optional<base::string16>, FieldPropertiesMask>>
      field_value_and_properties_map_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_FIELD_DATA_MANAGER_H_
