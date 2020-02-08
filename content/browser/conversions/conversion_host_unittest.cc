// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_host.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/fake_mojo_message_dispatch_context.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class ConversionHostTest : public RenderViewHostTestHarness {
 public:
  ConversionHostTest() {
    feature_list_.InitAndEnableFeature(features::kConversionMeasurement);
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    static_cast<WebContentsImpl*>(web_contents())
        ->RemoveReceiverSetForTesting(blink::mojom::ConversionHost::Name_);
    conversion_host_ = std::make_unique<ConversionHost>(web_contents());
    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
  }

  TestWebContents* contents() {
    return static_cast<TestWebContents*>(web_contents());
  }

  ConversionHost* conversion_host() { return conversion_host_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;
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

}  // namespace content
