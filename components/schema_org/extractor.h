// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEMA_ORG_EXTRACTOR_H_
#define COMPONENTS_SCHEMA_ORG_EXTRACTOR_H_

#include <string>

#include "components/schema_org/common/metadata.mojom-forward.h"

namespace schema_org {

// Extract structured metadata (schema.org in JSON-LD) from text content.
class Extractor {
 public:
  static mojom::EntityPtr Extract(const std::string& content);
};

}  // namespace schema_org

#endif  // COMPONENTS_SCHEMA_ORG_EXTRACTOR_H_
