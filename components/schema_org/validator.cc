// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/schema_org/validator.h"

#include <vector>

#include "components/schema_org/common/improved_metadata.mojom.h"
#include "components/schema_org/schema_org_entity_names.h"
#include "components/schema_org/schema_org_property_configurations.h"
#include "components/schema_org/schema_org_property_names.h"

namespace schema_org {

using improved::mojom::Entity;
using improved::mojom::EntityPtr;

// static
bool ValidateEntity(Entity* entity) {
  if (!entity::IsValidEntityName(entity->type)) {
    return false;
  }

  // Cycle through properties and remove any that have the wrong type.
  auto it = entity->properties.begin();
  while (it != entity->properties.end()) {
    property::PropertyConfiguration config =
        property::GetPropertyConfiguration((*it)->name);

    if (!(*it)->values->string_values.empty() && !config.text) {
      it = entity->properties.erase(it);
    } else if (!(*it)->values->double_values.empty() && !config.number) {
      it = entity->properties.erase(it);
    } else if (!(*it)->values->time_values.empty() && !config.time) {
      it = entity->properties.erase(it);
    } else if (!(*it)->values->date_time_values.empty() && !config.date_time &&
               !config.date) {
      it = entity->properties.erase(it);
    } else if (!(*it)->values->entity_values.empty()) {
      if (config.thing_types.empty()) {
        // Property is not supposed to have an entity type.
        it = entity->properties.erase(it);
      } else {
        // Check all the entities nested in this property. Remove any invalid
        // ones.
        bool has_valid_entities = false;
        auto nested_it = (*it)->values->entity_values.begin();
        while (nested_it != (*it)->values->entity_values.end()) {
          auto& nested_entity = *nested_it;
          if (!ValidateEntity(nested_entity.get())) {
            nested_it = (*it)->values->entity_values.erase(nested_it);
          } else {
            has_valid_entities = true;
            ++nested_it;
          }
        }

        // If there were no valid entity values for this property, remove the
        // whole property.
        if (!has_valid_entities) {
          it = entity->properties.erase(it);
        } else {
          ++it;
        }
      }
    } else {
      ++it;
    }
  }

  return true;
}

}  // namespace schema_org
