// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_client_policy.h"

namespace offline_pages {

OfflinePageClientPolicy::OfflinePageClientPolicy(std::string namespace_val,
                                                 LifetimeType lifetime_type_val)
    : name_space(namespace_val), lifetime_type(lifetime_type_val) {}

OfflinePageClientPolicy::OfflinePageClientPolicy(
    const OfflinePageClientPolicy& other) = default;

OfflinePageClientPolicy::~OfflinePageClientPolicy() = default;

}  // namespace offline_pages
