// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_embedder_policy.h"

namespace network {

CrossOriginEmbedderPolicy::CrossOriginEmbedderPolicy() = default;
CrossOriginEmbedderPolicy::CrossOriginEmbedderPolicy(
    const CrossOriginEmbedderPolicy& src) = default;
CrossOriginEmbedderPolicy::CrossOriginEmbedderPolicy(
    CrossOriginEmbedderPolicy&& src) = default;
CrossOriginEmbedderPolicy::~CrossOriginEmbedderPolicy() = default;

CrossOriginEmbedderPolicy& CrossOriginEmbedderPolicy::operator=(
    const CrossOriginEmbedderPolicy& src) = default;
CrossOriginEmbedderPolicy& CrossOriginEmbedderPolicy::operator=(
    CrossOriginEmbedderPolicy&& src) = default;

}  // namespace network
