// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/bind_test_util.h"
#include "components/services/patch/public/mojom/constants.mojom.h"
#include "components/services/patch/public/mojom/file_patcher.mojom.h"
#include "components/services/unzip/public/mojom/constants.mojom.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "content/public/browser/system_connector.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ServicesTest : public testing::Test {
 public:
  ServicesTest()
      : thread_bundle_(content::TestBrowserThreadBundle::MainThreadType::IO) {}

  template <typename Interface>
  bool CanAccessInterfaceFromBrowser(const std::string& service_name) {
    mojo::InterfacePtr<Interface> interface;
    connector()->BindInterface(service_name, mojo::MakeRequest(&interface));

    // If the service is present, the interface will be connected and
    // FlushForTesting will complete without an error on the interface.
    // Conversely if there is a problem connecting to the interface, we will
    // always hit the error handler before FlushForTesting returns.
    bool encountered_error = false;
    interface.set_connection_error_handler(
        base::BindLambdaForTesting([&] { encountered_error = true; }));
    interface.FlushForTesting();
    return !encountered_error;
  }

 private:
  service_manager::Connector* connector() {
    return content::GetSystemConnector();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper_;

  DISALLOW_COPY_AND_ASSIGN(ServicesTest);
};

}  // namespace

TEST_F(ServicesTest, ConnectToUnzip) {
  EXPECT_TRUE(CanAccessInterfaceFromBrowser<unzip::mojom::Unzipper>(
      unzip::mojom::kServiceName));
}

TEST_F(ServicesTest, ConnectToFilePatch) {
  EXPECT_TRUE(CanAccessInterfaceFromBrowser<patch::mojom::FilePatcher>(
      patch::mojom::kServiceName));
}
