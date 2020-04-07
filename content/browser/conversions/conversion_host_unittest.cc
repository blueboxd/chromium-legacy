// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_host.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "content/browser/conversions/conversion_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/fake_mojo_message_dispatch_context.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

const char kConversionUrl[] = "https://b.com";

Impression CreateValidImpression() {
  Impression result;
  result.conversion_destination = url::Origin::Create(GURL(kConversionUrl));
  result.reporting_origin = url::Origin::Create(GURL("https://c.com"));
  result.impression_data = 1UL;
  return result;
}

}  // namespace

class TestConversionManager : public ConversionManager {
 public:
  TestConversionManager() = default;
  ~TestConversionManager() override = default;

  void HandleImpression(const StorableImpression& impression) override {
    num_impressions_++;
  }

  void HandleConversion(const StorableConversion& impression) override {
    num_conversions_++;
  }

  const ConversionPolicy& GetConversionPolicy() const override {
    return policy;
  }

  size_t num_impressions() const { return num_impressions_; }

  void Reset() {
    num_impressions_ = 0u;
    num_conversions_ = 0u;
  }

 private:
  ConversionPolicy policy;
  size_t num_impressions_ = 0u;
  size_t num_conversions_ = 0u;
};

class TestManagerProvider : public ConversionManager::Provider {
 public:
  explicit TestManagerProvider(ConversionManager* manager)
      : manager_(manager) {}
  ~TestManagerProvider() override = default;

  ConversionManager* GetManager(WebContents* web_contents) const override {
    return manager_;
  }

 private:
  ConversionManager* manager_ = nullptr;
};

class ConversionHostTest : public RenderViewHostTestHarness {
 public:
  ConversionHostTest() = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    static_cast<WebContentsImpl*>(web_contents())
        ->RemoveReceiverSetForTesting(blink::mojom::ConversionHost::Name_);

    conversion_host_ = ConversionHost::CreateForTesting(
        web_contents(), std::make_unique<TestManagerProvider>(&test_manager_));
    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
  }

  TestWebContents* contents() {
    return static_cast<TestWebContents*>(web_contents());
  }

  ConversionHost* conversion_host() { return conversion_host_.get(); }

 protected:
  TestConversionManager test_manager_;

 private:
  std::unique_ptr<ConversionHost> conversion_host_;
};

TEST_F(ConversionHostTest, ConversionInSubframe_BadMessage) {
  contents()->NavigateAndCommit(GURL("http://www.example.com"));

  // Create a subframe and use it as a target for the conversion registration
  // mojo.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  conversion_host()->SetCurrentTargetFrameForTesting(subframe);

  // Create a fake dispatch context to trigger a bad message in.
  FakeMojoMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();

  // Message should be ignored because it was registered from a subframe.
  conversion_host()->RegisterConversion(std::move(conversion));
  EXPECT_EQ("blink.mojom.ConversionHost can only be used by the main frame.",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(ConversionHostTest, ConversionOnInsecurePage_BadMessage) {
  // Create a page with an insecure origin.
  contents()->NavigateAndCommit(GURL("http://www.example.com"));
  conversion_host()->SetCurrentTargetFrameForTesting(main_rfh());

  FakeMojoMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
  conversion->reporting_origin =
      url::Origin::Create(GURL("https://secure.com"));

  // Message should be ignored because it was registered from an insecure page.
  conversion_host()->RegisterConversion(std::move(conversion));
  EXPECT_EQ(
      "blink.mojom.ConversionHost can only be used in secure contexts with a "
      "secure conversion registration origin.",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(ConversionHostTest, ConversionWithInsecureReportingOrigin_BadMessage) {
  contents()->NavigateAndCommit(GURL("https://www.example.com"));
  conversion_host()->SetCurrentTargetFrameForTesting(main_rfh());

  FakeMojoMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
  conversion->reporting_origin = url::Origin::Create(GURL("http://secure.com"));

  // Message should be ignored because it was registered with an insecure
  // redirect.
  conversion_host()->RegisterConversion(std::move(conversion));
  EXPECT_EQ(
      "blink.mojom.ConversionHost can only be used in secure contexts with a "
      "secure conversion registration origin.",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(ConversionHostTest, ValidConversion_NoBadMessage) {
  // Create a page with an insecure origin.
  contents()->NavigateAndCommit(GURL("https://www.example.com"));
  conversion_host()->SetCurrentTargetFrameForTesting(main_rfh());

  // Create a fake dispatch context to trigger a bad message in.
  FakeMojoMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
  conversion->reporting_origin =
      url::Origin::Create(GURL("https://secure.com"));
  conversion_host()->RegisterConversion(std::move(conversion));

  // Run loop to allow the bad message code to run if a bad message was
  // triggered.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

TEST_F(ConversionHostTest, NavigationWithNoImpression_Ignored) {
  NavigationSimulatorImpl::NavigateAndCommitFromDocument(GURL(kConversionUrl),
                                                         main_rfh());

  EXPECT_EQ(0u, test_manager_.num_impressions());
}

TEST_F(ConversionHostTest, ValidImpression_ForwardedToManager) {
  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->set_impression(CreateValidImpression());
  navigation->Commit();

  EXPECT_EQ(1u, test_manager_.num_impressions());
}

TEST_F(ConversionHostTest, ImpressionWithNoManagerAvilable_NoCrash) {
  // Replace the ConversionHost on the WebContents with one that is backed by a
  // null ConversionManager.
  static_cast<WebContentsImpl*>(web_contents())
      ->RemoveReceiverSetForTesting(blink::mojom::ConversionHost::Name_);
  auto conversion_host = ConversionHost::CreateForTesting(
      web_contents(), std::make_unique<TestManagerProvider>(nullptr));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->set_impression(CreateValidImpression());
  navigation->Commit();
}

TEST_F(ConversionHostTest, ImpressionInSubframe_Ignored) {
  contents()->NavigateAndCommit(GURL("https://a.com"));

  // Create a subframe and use it as a target for the conversion registration
  // mojo.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), subframe);
  navigation->set_impression(CreateValidImpression());
  navigation->Commit();

  EXPECT_EQ(0u, test_manager_.num_impressions());
}

TEST_F(ConversionHostTest, ImpressionNavigationCommitsToErrorPage_Ignored) {
  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->set_impression(CreateValidImpression());
  navigation->Fail(net::ERR_FAILED);
  navigation->CommitErrorPage();

  EXPECT_EQ(0u, test_manager_.num_impressions());
}

TEST_F(ConversionHostTest, ImpressionNavigationAborts_Ignored) {
  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->set_impression(CreateValidImpression());
  navigation->AbortCommit();

  EXPECT_EQ(0u, test_manager_.num_impressions());
}

TEST_F(ConversionHostTest,
       CommittedOriginDiffersFromConversionDesintation_Ignored) {
  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL("https://different.com"), main_rfh());
  navigation->set_impression(CreateValidImpression());
  navigation->Commit();

  EXPECT_EQ(0u, test_manager_.num_impressions());
}

TEST_F(ConversionHostTest,
       ImpressionNavigation_OriginTrustworthyChecksPerformed) {
  const char kLocalHost[] = "http://localhost";

  struct {
    std::string conversion_origin;
    std::string reporting_origin;
    bool impression_expected;
  } kTestCases[] = {
      {kLocalHost /* conversion_origin */, kLocalHost /* reporting_origin */,
       true /* impression_expected */},
      {"http://127.0.0.1" /* conversion_origin */,
       "http://127.0.0.1" /* reporting_origin */,
       true /* impression_expected */},
      {kLocalHost /* conversion_origin */,
       "http://insecure.com" /* reporting_origin */,
       false /* impression_expected */},
      {"http://insecure.com" /* conversion_origin */,
       kLocalHost /* reporting_origin */, false /* impression_expected */},
      {"https://secure.com" /* conversion_origin */,
       "https://secure.com" /* reporting_origin */,
       true /* impression_expected */},
  };

  for (const auto& test_case : kTestCases) {
    auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
        GURL(test_case.conversion_origin), main_rfh());

    Impression impression;
    impression.conversion_destination =
        url::Origin::Create(GURL(test_case.conversion_origin));
    impression.reporting_origin =
        url::Origin::Create(GURL(test_case.reporting_origin));
    navigation->set_impression(impression);
    navigation->Commit();

    EXPECT_EQ(test_case.impression_expected, test_manager_.num_impressions())
        << "For test case: " << test_case.conversion_origin << " | "
        << test_case.reporting_origin;
    test_manager_.Reset();
  }
}

}  // namespace content
