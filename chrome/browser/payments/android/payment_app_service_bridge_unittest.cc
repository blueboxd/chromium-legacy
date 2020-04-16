// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/android/payment_app_service_bridge.h"

#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/test/base/testing_profile.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"

namespace payments {

class MockCallback {
 public:
  MockCallback() = default;
  MOCK_METHOD2(NotifyPaymentAppsCreated,
               void(const content::PaymentAppProvider::PaymentApps&,
                    const payments::ServiceWorkerPaymentAppFinder::
                        InstallablePaymentApps&));
  MOCK_METHOD1(NotifyPaymentAppCreationError, void(const std::string& error));
  MOCK_METHOD0(NotifyDoneCreatingPaymentApps, void(void));
};

class PaymentAppServiceBridgeUnitTest : public ::testing::Test {
 public:
  PaymentAppServiceBridgeUnitTest()
      : top_origin_("https://shop.example"),
        frame_origin_("https://merchant.example") {
    web_contents_ =
        test_web_contents_factory_.CreateWebContents(&browser_context_);
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents_,
                                                               frame_origin_);
  }

  mojom::PaymentMethodDataPtr MakePaymentMethodData(
      const std::string& supported_method) {
    mojom::PaymentMethodDataPtr out = mojom::PaymentMethodData::New();
    out->supported_method = supported_method;
    return out;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile browser_context_;
  content::TestWebContentsFactory test_web_contents_factory_;
  content::WebContents* web_contents_;
  GURL top_origin_;
  GURL frame_origin_;
  scoped_refptr<PaymentManifestWebDataService> web_data_service_;
};

TEST_F(PaymentAppServiceBridgeUnitTest, Smoke) {
  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(MakePaymentMethodData("basic-card"));
  method_data.push_back(MakePaymentMethodData("https://ph.example"));

  MockCallback mock_callback;
  base::WeakPtr<PaymentAppServiceBridge> bridge =
      PaymentAppServiceBridge::Create(
          /* number_of_factories= */ 3, web_contents_->GetMainFrame(),
          top_origin_, std::move(method_data), web_data_service_,
          /* may_crawl_for_installable_payment_apps= */ true,
          base::BindRepeating(&MockCallback::NotifyPaymentAppsCreated,
                              base::Unretained(&mock_callback)),
          base::BindRepeating(&MockCallback::NotifyPaymentAppCreationError,
                              base::Unretained(&mock_callback)),
          base::BindOnce(&MockCallback::NotifyDoneCreatingPaymentApps,
                         base::Unretained(&mock_callback)))
          ->GetWeakPtr();

  EXPECT_TRUE(bridge->SkipCreatingNativePaymentApps());
  EXPECT_EQ(web_contents_, bridge->GetWebContents());
  EXPECT_EQ(top_origin_, bridge->GetTopOrigin());
  EXPECT_EQ(frame_origin_, bridge->GetFrameOrigin());
  EXPECT_EQ("https://merchant.example",
            bridge->GetFrameSecurityOrigin().Serialize());
  EXPECT_EQ(web_contents_->GetMainFrame(),
            bridge->GetInitiatorRenderFrameHost());
  EXPECT_EQ(2U, bridge->GetMethodData().size());
  EXPECT_EQ("basic-card", bridge->GetMethodData()[0]->supported_method);
  EXPECT_EQ("https://ph.example", bridge->GetMethodData()[1]->supported_method);
  EXPECT_TRUE(bridge->MayCrawlForInstallablePaymentApps());

  content::PaymentAppProvider::PaymentApps apps;
  ServiceWorkerPaymentAppFinder::InstallablePaymentApps installables;

  EXPECT_CALL(mock_callback,
              NotifyPaymentAppsCreated(::testing::_, ::testing::_));
  bridge->OnCreatingNativePaymentAppsSkipped(std::move(apps),
                                             std::move(installables));

  EXPECT_CALL(mock_callback, NotifyPaymentAppCreationError("some error"));
  bridge->OnPaymentAppCreationError("some error");

  // NotifyDoneCreatingPaymentApps() is only called after
  // OnDoneCreatingPaymentApps() is called for each payment factories in
  // |bridge|.
  bridge->OnDoneCreatingPaymentApps();
  bridge->OnDoneCreatingPaymentApps();

  EXPECT_CALL(mock_callback, NotifyDoneCreatingPaymentApps());
  bridge->OnDoneCreatingPaymentApps();

  // |bridge| cleans itself up after NotifyDoneCreatingPaymentApps().
  CHECK_EQ(nullptr, bridge.get());
}

}  // namespace payments
