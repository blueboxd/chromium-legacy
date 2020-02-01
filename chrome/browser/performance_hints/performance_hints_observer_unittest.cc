// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_hints/performance_hints_observer.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gmock/include/gmock/gmock.h"

using optimization_guide::OptimizationGuideDecision;
using optimization_guide::proto::PerformanceHint;
using testing::Eq;

namespace {
const char kTestUrl[] = "http://www.test.com/";
}  // namespace

// TODO(crbug/1035698): Migrate to TestOptimizationGuideDecider when provided.
class MockOptimizationGuideKeyedService : public OptimizationGuideKeyedService {
 public:
  explicit MockOptimizationGuideKeyedService(
      content::BrowserContext* browser_context)
      : OptimizationGuideKeyedService(browser_context) {}
  ~MockOptimizationGuideKeyedService() override = default;

  MOCK_METHOD2(
      RegisterOptimizationTypesAndTargets,
      void(const std::vector<optimization_guide::proto::OptimizationType>&,
           const std::vector<optimization_guide::proto::OptimizationTarget>&));
  MOCK_METHOD2(ShouldTargetNavigation,
               optimization_guide::OptimizationGuideDecision(
                   content::NavigationHandle*,
                   optimization_guide::proto::OptimizationTarget));
  MOCK_METHOD3(CanApplyOptimization,
               optimization_guide::OptimizationGuideDecision(
                   content::NavigationHandle*,
                   optimization_guide::proto::OptimizationType,
                   optimization_guide::OptimizationMetadata*));
  MOCK_METHOD3(CanApplyOptimizationAsync,
               void(content::NavigationHandle*,
                    optimization_guide::proto::OptimizationType,
                    optimization_guide::OptimizationGuideDecisionCallback));
};

class PerformanceHintsObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  PerformanceHintsObserverTest() = default;
  ~PerformanceHintsObserverTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {kPerformanceHintsObserver,
         // Need to enable kOptimizationHints or GetForProfile will return
         // nullptr.
         optimization_guide::features::kOptimizationHints},
        {});

    ChromeRenderViewHostTestHarness::SetUp();
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();

    mock_optimization_guide_keyed_service_ =
        static_cast<MockOptimizationGuideKeyedService*>(
            OptimizationGuideKeyedServiceFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    profile(),
                    base::BindRepeating([](content::BrowserContext* context)
                                            -> std::unique_ptr<KeyedService> {
                      return std::make_unique<
                          MockOptimizationGuideKeyedService>(context);
                    })));

    test_handle_ = std::make_unique<content::MockNavigationHandle>(
        GURL(kTestUrl), main_rfh());
    std::vector<GURL> redirect_chain;
    redirect_chain.push_back(GURL(kTestUrl));
    test_handle_->set_redirect_chain(redirect_chain);
    test_handle_->set_has_committed(true);
    test_handle_->set_is_same_document(false);
    test_handle_->set_is_error_page(false);
  }

  void CallDidFinishNavigation(PerformanceHintsObserver* observer) {
    observer->DidFinishNavigation(test_handle_.get());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::MockNavigationHandle> test_handle_;
  MockOptimizationGuideKeyedService* mock_optimization_guide_keyed_service_ =
      nullptr;

  DISALLOW_COPY_AND_ASSIGN(PerformanceHintsObserverTest);
};

TEST_F(PerformanceHintsObserverTest, HasPerformanceHints) {
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              RegisterOptimizationTypesAndTargets(
                  testing::UnorderedElementsAre(
                      optimization_guide::proto::PERFORMANCE_HINTS),
                  testing::IsEmpty()));

  optimization_guide::OptimizationMetadata metadata;
  auto* hint = metadata.performance_hints_metadata.add_performance_hints();
  hint->set_wildcard_pattern("test.com");
  hint->set_performance_class(optimization_guide::proto::PERFORMANCE_SLOW);
  hint = metadata.performance_hints_metadata.add_performance_hints();
  hint->set_wildcard_pattern("othersite.net");
  hint->set_performance_class(optimization_guide::proto::PERFORMANCE_FAST);
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimizationAsync(
                  testing::_, optimization_guide::proto::PERFORMANCE_HINTS,
                  base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          testing::ByRef(metadata)));

  PerformanceHintsObserver::CreateForWebContents(web_contents());
  PerformanceHintsObserver* observer =
      PerformanceHintsObserver::FromWebContents(web_contents());
  ASSERT_NE(observer, nullptr);
  CallDidFinishNavigation(observer);

  base::HistogramTester histogram_tester;

  EXPECT_THAT(
      observer->HintForURL(GURL("http://www.test.com")),
      testing::Optional(testing::AllOf(
          testing::Property(&PerformanceHint::wildcard_pattern, Eq("test.com")),
          testing::Property(&PerformanceHint::performance_class,
                            Eq(optimization_guide::proto::PERFORMANCE_SLOW)))));
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult", /*kHintFound*/ 3, 1);
  EXPECT_THAT(
      observer->HintForURL(GURL("https://www.othersite.net/this/link")),
      testing::Optional(testing::AllOf(
          testing::Property(&PerformanceHint::wildcard_pattern,
                            Eq("othersite.net")),
          testing::Property(&PerformanceHint::performance_class,
                            Eq(optimization_guide::proto::PERFORMANCE_FAST)))));
  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult", /*kHintFound*/ 3, 2);
  EXPECT_THAT(observer->HintForURL(GURL("https://www.nohint.com")),
              Eq(base::nullopt));
  histogram_tester.ExpectBucketCount(
      "PerformanceHints.Observer.HintForURLResult", /*kHintNotFound*/ 0, 1);
}

TEST_F(PerformanceHintsObserverTest, NoHintsForPage) {
  optimization_guide::OptimizationMetadata metadata;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimizationAsync(
                  testing::_, optimization_guide::proto::PERFORMANCE_HINTS,
                  base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kFalse,
          testing::ByRef(metadata)));

  PerformanceHintsObserver::CreateForWebContents(web_contents());
  PerformanceHintsObserver* observer =
      PerformanceHintsObserver::FromWebContents(web_contents());
  ASSERT_NE(observer, nullptr);
  CallDidFinishNavigation(observer);

  base::HistogramTester histogram_tester;

  EXPECT_THAT(observer->HintForURL(GURL("https://www.nohint.com")),
              Eq(base::nullopt));

  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult", /*kHintNotFound*/ 0, 1);
}

TEST_F(PerformanceHintsObserverTest, PerformanceInfoRequestedBeforeCallback) {
  optimization_guide::OptimizationMetadata metadata;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimizationAsync(
                  testing::_, optimization_guide::proto::PERFORMANCE_HINTS,
                  base::test::IsNotNullCallback()));

  PerformanceHintsObserver::CreateForWebContents(web_contents());
  PerformanceHintsObserver* observer =
      PerformanceHintsObserver::FromWebContents(web_contents());
  ASSERT_NE(observer, nullptr);
  CallDidFinishNavigation(observer);

  base::HistogramTester histogram_tester;

  EXPECT_THAT(observer->HintForURL(GURL("https://www.nohint.com")),
              Eq(base::nullopt));

  histogram_tester.ExpectUniqueSample(
      "PerformanceHints.Observer.HintForURLResult", /*kHintNotReady*/ 1, 1);
}

TEST_F(PerformanceHintsObserverTest, OptimizationGuideDisabled) {
  mock_optimization_guide_keyed_service_ = nullptr;
  OptimizationGuideKeyedServiceFactory::GetInstance()->SetTestingFactory(
      profile(), OptimizationGuideKeyedServiceFactory::TestingFactory());

  PerformanceHintsObserver::CreateForWebContents(web_contents());
  PerformanceHintsObserver* observer =
      PerformanceHintsObserver::FromWebContents(web_contents());
  ASSERT_NE(observer, nullptr);
  CallDidFinishNavigation(observer);

  EXPECT_THAT(observer->HintForURL(GURL("http://www.test.com")),
              Eq(base::nullopt));
}

TEST_F(PerformanceHintsObserverTest, NoErrorPageHints) {
  test_handle_->set_is_error_page(true);

  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimizationAsync(testing::_, testing::_, testing::_))
      .Times(0);

  PerformanceHintsObserver::CreateForWebContents(web_contents());
  PerformanceHintsObserver* observer =
      PerformanceHintsObserver::FromWebContents(web_contents());
  ASSERT_NE(observer, nullptr);
  CallDidFinishNavigation(observer);

  EXPECT_THAT(observer->HintForURL(GURL("http://www.test.com")),
              Eq(base::nullopt));
}

TEST_F(PerformanceHintsObserverTest, DontFetchForSubframe) {
  test_handle_ = std::make_unique<content::MockNavigationHandle>(
      GURL(kTestUrl),
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe"));
  std::vector<GURL> redirect_chain;
  redirect_chain.push_back(GURL(kTestUrl));
  test_handle_->set_redirect_chain(redirect_chain);
  test_handle_->set_has_committed(true);
  test_handle_->set_is_same_document(false);
  test_handle_->set_is_error_page(false);

  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimizationAsync(testing::_, testing::_, testing::_))
      .Times(0);

  PerformanceHintsObserver::CreateForWebContents(web_contents());
  PerformanceHintsObserver* observer =
      PerformanceHintsObserver::FromWebContents(web_contents());
  ASSERT_NE(observer, nullptr);
  CallDidFinishNavigation(observer);

  EXPECT_THAT(observer->HintForURL(GURL("http://www.test.com")),
              Eq(base::nullopt));
}
