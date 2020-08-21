// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/server_printers_provider.h"

#include <memory>
#include <string>

#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/printing/print_server.h"
#include "chrome/browser/chromeos/printing/print_servers_provider.h"
#include "chrome/browser/chromeos/printing/print_servers_provider_factory.h"
#include "chrome/browser/chromeos/printing/printer_detector.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libipp/libipp/ipp.h"

using ::testing::AllOf;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::UnorderedElementsAre;

namespace chromeos {

namespace {

const char kAccountName[] = "test";

// An example of configuration file with print servers for user policy.
const char kUserExternalPrintServersContentsJson[] = R"json(
[
  {
    "id": "id1",
    "display_name": "LexaPrint - User",
    "url": "ipp://192.168.1.5/user-printer",
  }, {
    "id": "id2",
    "display_name": "Color Laser - User",
    "url":"ipps://user-print-server.intranet.example.com:443/ipp/cl2k4",
  }, {
    "id": "id3",
    "display_name": "B&W Printer - User",
    "url":"ipps://user-print-server.intranet.example.com:443/bwprinter",
  }
])json";

Printer UserPrinter1() {
  Printer printer("server-20e91b728d4d04bc68132ced81772ef5");
  printer.set_display_name("LexaPrint - User Name");
  std::string server("ipp://192.168.1.5");
  printer.set_print_server_uri(server);
  Uri url("ipp://192.168.1.5:631/printers/LexaPrint - User Name");
  printer.SetUri(url);
  return printer;
}

Printer UserPrinter2() {
  Printer printer("server-5da95e01216b1fe0ee1de25dc8d0a6e8");
  printer.set_display_name("Color Laser - User Name");
  std::string server("ipps://user-print-server.intranet.example.com");
  printer.set_print_server_uri(server);
  Uri url(
      "ipps://user-print-server.intranet.example.com:443/printers/"
      "Color Laser "
      "- User Name");
  printer.SetUri(url);
  return printer;
}

// An example of configuration file with print servers for device policy.
const char kDeviceExternalPrintServersContentsJson[] = R"json(
[
  {
    "id": "id1",
    "display_name": "LexaPrint - Device",
    "url": "ipp://192.168.1.5/device-printer",
  }, {
    "id": "id2",
    "display_name": "Color Laser - Device",
    "url":"ipps://device-print-server.intranet.example.com:443/ipp/cl2k4",
  }, {
    "id": "id3",
    "display_name": "B&W Printer - Device",
    "url":"ipps://device-print-server.intranet.example.com:443/bwprinter",
  }
])json";

// An example allowlist for device policy.
const std::vector<std::string> kDevicePrintServersPolicyAllowlist = {
    "id3", "idX", "id1"};

Printer DevicePrinter1() {
  Printer printer("server-f4a2ce25d8f9e6335d36f8253f8cf047");
  printer.set_display_name("LexaPrint - Device Name");
  std::string server("ipp://192.168.1.5");
  Uri url("ipp://192.168.1.5:631/printers/LexaPrint - Device Name");
  printer.set_print_server_uri(server);
  printer.SetUri(url);
  return printer;
}

Printer DevicePrinter2() {
  Printer printer("server-1f88fe69dd2ce98ae6c195f3eb295a6d");
  printer.set_display_name("B&W Printer - Device Name");
  std::string server("ipps://device-print-server.intranet.example.com");
  printer.set_print_server_uri(server);
  Uri url(
      "ipps://device-print-server.intranet.example.com:443/printers/"
      "B&W Printer - Device Name");
  printer.SetUri(url);
  return printer;
}

}  // namespace

auto GetPrinter = [](const PrinterDetector::DetectedPrinter& input) -> Printer {
  return input.printer;
};

auto PrinterMatcher(Printer printer) {
  return ResultOf(
      GetPrinter,
      AllOf(Property(&Printer::uri, printer.uri()),
            Property(&Printer::print_server_uri, printer.print_server_uri()),
            Property(&Printer::display_name, printer.display_name())));
}

class ServerPrintersProviderTest : public ::testing::Test {
 public:
  ServerPrintersProviderTest()
      : local_state_(TestingBrowserProcess::GetGlobal()),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

 protected:
  void SetUp() override {
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_shared_loader_factory_);

    ASSERT_TRUE(test_server_.Start());

    SetupUserProfile();

    server_printers_provider_ = ServerPrintersProvider::Create(profile_.get());
  }

  void SetupUserProfile() {
    auto unique_user_manager = std::make_unique<FakeChromeUserManager>();
    auto* user_manager = unique_user_manager.get();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(unique_user_manager));

    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(kAccountName);
    profile_ = profile_builder.Build();
    user_manager->AddUserWithAffiliationAndTypeAndProfile(
        AccountId::FromUserEmail(kAccountName), false,
        user_manager::UserType::USER_TYPE_REGULAR, profile_.get());
  }

  void TearDown() override { PrintServersProviderFactory::Get()->Shutdown(); }

  std::string CreateResponse(const std::string& name,
                             const std::string& description) {
    ipp::Response_CUPS_Get_Printers response;
    response.printer_attributes[0].printer_name.Set(
        ipp::StringWithLanguage(name, "us-EN"));
    response.printer_attributes[0].printer_info.Set(
        ipp::StringWithLanguage(description, "us-EN"));
    ipp::Server server(ipp::Version::_1_1, 1);
    server.BuildResponseFrom(&response);
    std::vector<uint8_t> bin_data;
    EXPECT_TRUE(server.WriteResponseFrameTo(&bin_data));
    std::string response_body(bin_data.begin(), bin_data.end());
    return response_body;
  }

  void ApplyDevicePolicy() {
    device_print_servers_provider_ =
        PrintServersProviderFactory::Get()->GetForDevice();
    device_print_servers_provider_->SetData(
        std::make_unique<std::string>(kDeviceExternalPrintServersContentsJson));
    // Apply device allowlist.
    auto device_allowlist =
        std::make_unique<base::Value>(base::Value::Type::LIST);

    for (const std::string& id : kDevicePrintServersPolicyAllowlist)
      device_allowlist->Append(base::Value(id));
    local_state_.Get()->SetManagedPref(
        prefs::kDeviceExternalPrintServersAllowlist,
        std::move(device_allowlist));
  }

  void ApplyUserPolicy() {
    static const std::vector<std::string> kUserPrintServersPolicyAllowlist = {
        "idX", "id2", "id1"};
    user_print_servers_provider_ =
        PrintServersProviderFactory::Get()->GetForProfile(profile_.get());
    user_print_servers_provider_->SetData(
        std::make_unique<std::string>(kUserExternalPrintServersContentsJson));
    // Apply user allowlist.
    auto user_allowlist = std::make_unique<base::ListValue>();
    for (const std::string& id : kUserPrintServersPolicyAllowlist)
      user_allowlist->Append(base::Value(id));
    profile_->GetTestingPrefService()->SetManagedPref(
        prefs::kExternalPrintServersAllowlist, std::move(user_allowlist));
  }

  // Everything must be called on Chrome_UIThread.
  content::BrowserTaskEnvironment task_environment_;

  ScopedTestingLocalState local_state_;

  network::TestURLLoaderFactory test_url_loader_factory_;

  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      test_shared_loader_factory_;

  std::unique_ptr<TestingProfile> profile_;

  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;

  net::test_server::EmbeddedTestServer test_server_;

  base::WeakPtr<PrintServersProvider> user_print_servers_provider_;
  base::WeakPtr<PrintServersProvider> device_print_servers_provider_;

  std::unique_ptr<ServerPrintersProvider> server_printers_provider_;
};

TEST_F(ServerPrintersProviderTest, GetPrinters_OnlyDevicePolicy) {
  test_url_loader_factory_.AddResponse(
      "http://192.168.1.5:631/device-printer",
      CreateResponse("LexaPrint - Device Name", "LexaPrint Description"));
  test_url_loader_factory_.AddResponse(
      "https://device-print-server.intranet.example.com:443/bwprinter",
      CreateResponse("B&W Printer - Device Name", "B&W Printer Description"));

  EXPECT_TRUE(server_printers_provider_->GetPrinters().empty());

  ApplyDevicePolicy();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(server_printers_provider_->GetPrinters(),
              UnorderedElementsAre(PrinterMatcher(DevicePrinter1()),
                                   PrinterMatcher(DevicePrinter2())));
}

TEST_F(ServerPrintersProviderTest, GetPrinters_OnlyUserPolicy) {
  test_url_loader_factory_.AddResponse(
      "http://192.168.1.5:631/user-printer",
      CreateResponse("LexaPrint - User Name", "LexaPrint Description"));
  test_url_loader_factory_.AddResponse(
      "https://user-print-server.intranet.example.com/ipp/cl2k4",
      CreateResponse("Color Laser - User Name", "Color Laser Description"));

  EXPECT_TRUE(server_printers_provider_->GetPrinters().empty());

  ApplyUserPolicy();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(server_printers_provider_->GetPrinters(),
              UnorderedElementsAre(PrinterMatcher(UserPrinter1()),
                                   PrinterMatcher(UserPrinter2())));
}

TEST_F(ServerPrintersProviderTest, GetPrinters_UserAndDevicePolicy) {
  test_url_loader_factory_.AddResponse(
      "http://192.168.1.5:631/device-printer",
      CreateResponse("LexaPrint - Device Name", "LexaPrint Description"));
  test_url_loader_factory_.AddResponse(
      "https://device-print-server.intranet.example.com:443/bwprinter",
      CreateResponse("B&W Printer - Device Name", "B&W Printer Description"));
  test_url_loader_factory_.AddResponse(
      "http://192.168.1.5:631/user-printer",
      CreateResponse("LexaPrint - User Name", "LexaPrint Description"));
  test_url_loader_factory_.AddResponse(
      "https://user-print-server.intranet.example.com/ipp/cl2k4",
      CreateResponse("Color Laser - User Name", "Color Laser Description"));

  EXPECT_TRUE(server_printers_provider_->GetPrinters().empty());

  ApplyUserPolicy();
  ApplyDevicePolicy();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(server_printers_provider_->GetPrinters(),
              UnorderedElementsAre(PrinterMatcher(DevicePrinter1()),
                                   PrinterMatcher(DevicePrinter2()),
                                   PrinterMatcher(UserPrinter1()),
                                   PrinterMatcher(UserPrinter2())));
}

}  // namespace chromeos
