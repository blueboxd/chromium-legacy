// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/persistence/site_data/site_data_cache_factory.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

TEST(SiteDataCacheFactoryTest, EndToEnd) {
  content::BrowserTaskEnvironment task_environment;
  auto performance_manager = PerformanceManagerImpl::Create(base::DoNothing());
  std::unique_ptr<SiteDataCacheFactory> factory =
      std::make_unique<SiteDataCacheFactory>();
  SiteDataCacheFactory* factory_raw = factory.get();
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<SiteDataCacheFactory> site_data_cache_factory,
             performance_manager::GraphImpl* graph) {
            graph->PassToGraph(std::move(site_data_cache_factory));
          },
          std::move(factory)));

  content::TestBrowserContext browser_context;
  SiteDataCacheFactory::OnBrowserContextCreatedOnUIThread(
      factory_raw, &browser_context, nullptr);

  {
    base::RunLoop run_loop;
    PerformanceManagerImpl::CallOnGraphImpl(
        FROM_HERE,
        base::BindOnce(
            [](SiteDataCacheFactory* factory,
               const std::string& browser_context_id,
               base::OnceClosure quit_closure) {
              DCHECK_NE(nullptr, factory->GetDataCacheForBrowserContext(
                                     browser_context_id));
              DCHECK_NE(nullptr, factory->GetInspectorForBrowserContext(
                                     browser_context_id));
              std::move(quit_closure).Run();
            },
            base::Unretained(factory_raw), browser_context.UniqueId(),
            run_loop.QuitClosure()));
    run_loop.Run();
  }

  SiteDataCacheFactory::OnBrowserContextDestroyedOnUIThread(factory_raw,
                                                            &browser_context);
  {
    base::RunLoop run_loop;
    PerformanceManagerImpl::CallOnGraphImpl(
        FROM_HERE,
        base::BindOnce(
            [](SiteDataCacheFactory* factory,
               const std::string& browser_context_id,
               base::OnceClosure quit_closure) {
              DCHECK_EQ(nullptr, factory->GetDataCacheForBrowserContext(
                                     browser_context_id));
              DCHECK_EQ(nullptr, factory->GetInspectorForBrowserContext(
                                     browser_context_id));
              std::move(quit_closure).Run();
            },
            base::Unretained(factory_raw), browser_context.UniqueId(),
            run_loop.QuitClosure()));
    run_loop.Run();
  }

  PerformanceManagerImpl::Destroy(std::move(performance_manager));
  task_environment.RunUntilIdle();
}

}  // namespace performance_manager
