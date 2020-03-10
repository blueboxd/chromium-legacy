// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEMA_ORG_VALIDATOR_H_
#define COMPONENTS_SCHEMA_ORG_VALIDATOR_H_

#include "components/schema_org/common/improved_metadata.mojom-forward.h"

namespace schema_org {

// Validates and cleans up the Schema.org entity in-place. Invalid properties
// will be removed from the entity. Returns true if the entity was valid.
bool ValidateEntity(improved::mojom::Entity* entity);

}  // namespace schema_org

#endif  // COMPONENTS_SCHEMA_ORG_VALIDATOR_H_
