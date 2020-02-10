// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRDTP_SERIALIZABLE_H_
#define CRDTP_SERIALIZABLE_H_

#include <cstdint>
#include <vector>
#include "export.h"

namespace crdtp {
// =============================================================================
// Serializable - An object to be emitted as a sequence of bytes.
// =============================================================================

class CRDTP_EXPORT Serializable {
 public:
  // Convenience: Invokes |AppendSerialized| on an empty vector.
  std::vector<uint8_t> Serialize() const;

  virtual void AppendSerialized(std::vector<uint8_t>* out) const = 0;

  virtual ~Serializable() = default;
};
}  // namespace crdtp

#endif  // CRDTP_SERIALIZABLE_H_
