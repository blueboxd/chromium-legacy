// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_embedder_policy.h"

namespace network {

CrossOriginEmbedderPolicyWithReporting::
    CrossOriginEmbedderPolicyWithReporting() = default;
CrossOriginEmbedderPolicyWithReporting::CrossOriginEmbedderPolicyWithReporting(
    const CrossOriginEmbedderPolicyWithReporting& src) = default;
CrossOriginEmbedderPolicyWithReporting::CrossOriginEmbedderPolicyWithReporting(
    CrossOriginEmbedderPolicyWithReporting&& src) = default;
CrossOriginEmbedderPolicyWithReporting::
    ~CrossOriginEmbedderPolicyWithReporting() = default;

CrossOriginEmbedderPolicyWithReporting& CrossOriginEmbedderPolicyWithReporting::
operator=(const CrossOriginEmbedderPolicyWithReporting& src) = default;
CrossOriginEmbedderPolicyWithReporting& CrossOriginEmbedderPolicyWithReporting::
operator=(CrossOriginEmbedderPolicyWithReporting&& src) = default;

}  // namespace network
