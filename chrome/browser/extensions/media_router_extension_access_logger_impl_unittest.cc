// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/media_router_extension_access_logger_impl.h"

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {
namespace {

// ID of the UKM that should be generated.
constexpr char kCastExtentensionLoadedUkmId[] =
    "MediaRouter.CastWebSenderExtensionLoadUrl";

// ID of the UKM that should be generated.
constexpr char kCastExtentensionLoadedUmaId[] =
    "MediaRouter.CastWebSenderExtensionLoaded";

}  // namespace

TEST(MediaRouterExtensionAccessLoggerImplTest, LogsUkmCorrectly) {
  content::BrowserTaskEnvironment task_environment;
  GURL source_url("http://foobar.com");

  // Mock the Metrics infra so testing can be done.
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester uma_recorder;

  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(HistoryServiceFactory::GetInstance(),
                                    HistoryServiceFactory::GetDefaultFactory());
  std::unique_ptr<TestingProfile> testing_profile = profile_builder.Build();
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(testing_profile.get(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  history_service->AddPage(source_url, base::Time::Now(),
                           history::SOURCE_BROWSED);

  base::RunLoop run_loop;
  history_service->set_origin_queried_closure_for_testing(
      run_loop.QuitClosure());

  // Run the test.
  content::BrowserContext* context = testing_profile.get();
  auto origin = url::Origin::Create(source_url);

  {
    MediaRouterExtensionAccessLoggerImpl metrics_logger;
    metrics_logger.LogMediaRouterComponentExtensionUse(origin, context);
  }

  run_loop.Run();

  // Do validations.
  EXPECT_EQ(1U, ukm_recorder.sources_count());
  EXPECT_EQ(1U, ukm_recorder.entries_count());
  const std::vector<const ukm::mojom::UkmEntry*> entries =
      ukm_recorder.GetEntriesByName(kCastExtentensionLoadedUkmId);
  EXPECT_EQ(1U, entries.size());

  uma_recorder.ExpectTotalCount(kCastExtentensionLoadedUmaId, 1);
}

}  // namespace extensions
