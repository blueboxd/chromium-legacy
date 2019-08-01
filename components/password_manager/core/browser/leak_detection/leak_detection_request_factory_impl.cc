// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/leak_detection_request_factory_impl.h"

#include "components/password_manager/core/browser/leak_detection/authenticated_leak_check.h"
#include "components/password_manager/core/common/password_manager_features.h"

namespace password_manager {

LeakDetectionRequestFactoryImpl::LeakDetectionRequestFactoryImpl() = default;
LeakDetectionRequestFactoryImpl::~LeakDetectionRequestFactoryImpl() = default;

std::unique_ptr<LeakDetectionCheck>
LeakDetectionRequestFactoryImpl::TryCreateLeakCheck() const {
  if (!base::FeatureList::IsEnabled(features::kLeakDetection))
    return nullptr;
  return std::make_unique<AuthenticatedLeakCheck>();
}

}  // namespace password_manager