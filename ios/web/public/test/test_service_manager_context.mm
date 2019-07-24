// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/test_service_manager_context.h"

#include "ios/web/service/service_manager_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

TestServiceManagerContext::TestServiceManagerContext()
    : context_(std::make_unique<ServiceManagerContext>()) {}

TestServiceManagerContext::~TestServiceManagerContext() = default;

}  // namespace web