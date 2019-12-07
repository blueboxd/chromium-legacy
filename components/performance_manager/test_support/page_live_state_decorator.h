// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PAGE_LIVE_STATE_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PAGE_LIVE_STATE_DECORATOR_H_

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"

namespace performance_manager {

// Helper function that allows testing that a PageLiveStateDecorator::Data
// property has the expected value. This function should be called from the main
// thread and be passed the WebContents pointer associated with the PageNode to
// check.
void TestPageLiveStatePropertyOnPMSequence(
    content::WebContents* contents,
    bool (PageLiveStateDecorator::Data::*getter)() const,
    bool expected_value);

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PAGE_LIVE_STATE_DECORATOR_H_
