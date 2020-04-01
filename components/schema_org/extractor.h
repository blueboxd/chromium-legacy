// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEMA_ORG_EXTRACTOR_H_
#define COMPONENTS_SCHEMA_ORG_EXTRACTOR_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/values.h"
#include "components/schema_org/common/improved_metadata.mojom-forward.h"

namespace schema_org {

// Extract structured metadata (schema.org in JSON-LD) from text content.
class Extractor {
 public:
  explicit Extractor(base::flat_set<base::StringPiece> supported_entity_types);
  Extractor(const Extractor&) = delete;
  Extractor& operator=(const Extractor&) = delete;
  ~Extractor();

  improved::mojom::EntityPtr Extract(const std::string& content);

 private:
  bool IsSupportedType(const std::string& type);
  improved::mojom::EntityPtr ExtractTopLevelEntity(
      const base::DictionaryValue& val);

  const base::flat_set<base::StringPiece> supported_types_;
};

}  // namespace schema_org

#endif  // COMPONENTS_SCHEMA_ORG_EXTRACTOR_H_
