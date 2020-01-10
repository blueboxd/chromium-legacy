// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/url_keyed_hint.h"

URLKeyedHint::URLKeyedHint(const base::Time expiration_time,
                           optimization_guide::proto::Hint&& url_keyed_hint)
    : expiration_time_(expiration_time),
      url_keyed_hint_(
          std::make_unique<optimization_guide::proto::Hint>(url_keyed_hint)) {}

URLKeyedHint::~URLKeyedHint() = default;