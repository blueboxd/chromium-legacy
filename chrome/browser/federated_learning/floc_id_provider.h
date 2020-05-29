// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEDERATED_LEARNING_FLOC_ID_PROVIDER_H_
#define CHROME_BROWSER_FEDERATED_LEARNING_FLOC_ID_PROVIDER_H_

#include "components/federated_learning/floc_id.h"
#include "components/keyed_service/core/keyed_service.h"

namespace federated_learning {

// KeyedService which computes the floc id regularly, and notifies relevant
// components about the updated id.
class FlocIdProvider : public KeyedService {
 public:
  ~FlocIdProvider() override = default;
};

}  // namespace federated_learning

#endif  // CHROME_BROWSER_FEDERATED_LEARNING_FLOC_ID_PROVIDER_H_
