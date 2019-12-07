// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/page_live_state_decorator.h"

#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

void TestPageLiveStatePropertyOnPMSequence(
    content::WebContents* contents,
    bool (PageLiveStateDecorator::Data::*getter)() const,
    bool expected_value) {
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  base::WeakPtr<PageNode> node =
      PerformanceManager::GetPageNodeForWebContents(contents);

  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting([&](Graph* unused) {
        EXPECT_TRUE(node);
        auto* data =
            PageLiveStateDecorator::Data::GetOrCreateForTesting(node.get());
        EXPECT_TRUE(data);
        EXPECT_EQ((data->*getter)(), expected_value);
        std::move(quit_closure).Run();
      }));
  run_loop.Run();
}

}  // namespace performance_manager
