// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_lorgnette_manager_client.h"
#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"
#include "extensions/browser/api/document_scan/document_scan_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/api_unittest.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/lorgnette/dbus-constants.h"

namespace extensions {

namespace api {

namespace {

lorgnette::ListScannersResponse CreateListScannersResponse() {
  lorgnette::ScannerInfo scanner;
  scanner.set_name("Dank Scanner");
  scanner.set_manufacturer("Scanners, Inc.");
  scanner.set_model("TX1000");
  scanner.set_type("Flatbed");
  lorgnette::ListScannersResponse response;
  *response.add_scanners() = std::move(scanner);
  return response;
}

}  // namespace

class DocumentScanScanFunctionTest : public ApiUnitTest {
 public:
  DocumentScanScanFunctionTest()
      : function_(base::MakeRefCounted<DocumentScanScanFunction>()) {}
  ~DocumentScanScanFunctionTest() override {}

  void SetUp() override {
    ApiUnitTest::SetUp();
    chromeos::DBusThreadManager::Initialize();
    function_->set_user_gesture(true);
  }

  void TearDown() override {
    chromeos::DBusThreadManager::Shutdown();
    ApiUnitTest::TearDown();
  }

  chromeos::FakeLorgnetteManagerClient* GetLorgnetteManagerClient() {
    return static_cast<chromeos::FakeLorgnetteManagerClient*>(
        chromeos::DBusThreadManager::Get()->GetLorgnetteManagerClient());
  }

 protected:
  std::string RunFunctionAndReturnError(const std::string& args) {
    function_->set_extension(extension());
    std::string error = api_test_utils::RunFunctionAndReturnError(
        function_.get(), args, browser_context(), api_test_utils::NONE);
    return error;
  }

  scoped_refptr<DocumentScanScanFunction> function_;
};

TEST_F(DocumentScanScanFunctionTest, UserGestureRequiredError) {
  function_->set_user_gesture(false);
  EXPECT_EQ("User gesture required to perform scan",
            RunFunctionAndReturnError("[{}]"));
}

TEST_F(DocumentScanScanFunctionTest, ListScannersError) {
  GetLorgnetteManagerClient()->SetListScannersResponse(base::nullopt);
  EXPECT_EQ("Failed to obtain list of scanners",
            RunFunctionAndReturnError("[{}]"));
}

TEST_F(DocumentScanScanFunctionTest, NoScannersAvailableError) {
  lorgnette::ListScannersResponse response;
  GetLorgnetteManagerClient()->SetListScannersResponse(response);
  EXPECT_EQ("No scanners available", RunFunctionAndReturnError("[{}]"));
}

TEST_F(DocumentScanScanFunctionTest, UnsupportedMimeTypesError) {
  GetLorgnetteManagerClient()->SetListScannersResponse(
      CreateListScannersResponse());
  EXPECT_EQ("Unsupported MIME types",
            RunFunctionAndReturnError("[{\"mimeTypes\": [\"image/tiff\"]}]"));
}

TEST_F(DocumentScanScanFunctionTest, ScanImageError) {
  GetLorgnetteManagerClient()->SetListScannersResponse(
      CreateListScannersResponse());
  GetLorgnetteManagerClient()->SetScanImageToStringResponse(base::nullopt);
  EXPECT_EQ("Failed to scan image",
            RunFunctionAndReturnError("[{\"mimeTypes\": [\"image/png\"]}]"));
}

TEST_F(DocumentScanScanFunctionTest, Success) {
  GetLorgnetteManagerClient()->SetListScannersResponse(
      CreateListScannersResponse());
  GetLorgnetteManagerClient()->SetScanImageToStringResponse("PrettyPicture");
  std::unique_ptr<base::DictionaryValue> result(RunFunctionAndReturnDictionary(
      function_.get(), "[{\"mimeTypes\": [\"image/png\"]}]"));
  ASSERT_NE(nullptr, result.get());
  document_scan::ScanResults scan_results;
  EXPECT_TRUE(document_scan::ScanResults::Populate(*result, &scan_results));
  // Verify the image data URL is the PNG image data URL prefix plus the base64
  // representation of "PrettyPicture".
  EXPECT_THAT(
      scan_results.data_urls,
      testing::ElementsAre("data:image/png;base64,UHJldHR5UGljdHVyZQ=="));
  EXPECT_EQ("image/png", scan_results.mime_type);
}

}  // namespace api

}  // namespace extensions
