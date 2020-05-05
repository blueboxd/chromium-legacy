// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/webui/settings/chromeos/device_dlc_handler.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/dbus/dlcservice/fake_dlcservice_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {

namespace {

dlcservice::DlcModuleList CreateDlcModuleListOfSize(size_t numDlcs) {
  dlcservice::DlcModuleList dlc_module_list;
  dlcservice::DlcModuleInfo* dlc_module_info;
  for (size_t i = 0; i < numDlcs; i++) {
    dlc_module_info = dlc_module_list.add_dlc_module_infos();
    dlc_module_info->set_dlc_id("dlcId" + base::NumberToString(i));
  }
  return dlc_module_list;
}

class TestDlcHandler : public DlcHandler {
 public:
  TestDlcHandler() = default;
  ~TestDlcHandler() override = default;

  // Make public for testing.
  using DlcHandler::AllowJavascript;
  using DlcHandler::set_web_ui;
};

class DlcHandlerTest : public testing::Test {
 public:
  DlcHandlerTest() = default;
  ~DlcHandlerTest() override = default;
  DlcHandlerTest(const DlcHandlerTest&) = delete;
  DlcHandlerTest& operator=(const DlcHandlerTest&) = delete;

  void SetUp() override {
    test_web_ui_ = std::make_unique<content::TestWebUI>();

    handler_ = std::make_unique<TestDlcHandler>();
    handler_->set_web_ui(test_web_ui_.get());
    handler_->RegisterMessages();
    handler_->AllowJavascriptForTesting();

    chromeos::DlcserviceClient::InitializeFake();
    fake_dlcservice_client_ = static_cast<chromeos::FakeDlcserviceClient*>(
        chromeos::DlcserviceClient::Get());
  }

  void TearDown() override {
    handler_.reset();
    test_web_ui_.reset();
    chromeos::DlcserviceClient::Shutdown();
  }

  content::TestWebUI* test_web_ui() { return test_web_ui_.get(); }

 protected:
  chromeos::FakeDlcserviceClient* fake_dlcservice_client_;
  std::unique_ptr<TestDlcHandler> handler_;
  std::unique_ptr<content::TestWebUI> test_web_ui_;
  content::BrowserTaskEnvironment task_environment_;

  const content::TestWebUI::CallData& CallDataAtIndex(size_t index) {
    return *test_web_ui_->call_data()[index];
  }

  size_t CallGetDlcListAndReturnSize() {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    base::ListValue args;
    args.AppendString("handlerFunctionName");
    test_web_ui()->HandleReceivedMessage("getDlcList", &args);
    task_environment_.RunUntilIdle();

    EXPECT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());

    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIResponse", call_data.function_name());
    EXPECT_EQ("handlerFunctionName", call_data.arg1()->GetString());
    return call_data.arg3()->GetList().size();
  }
};

TEST_F(DlcHandlerTest, GetDlcList) {
  fake_dlcservice_client_->set_installed_dlcs(CreateDlcModuleListOfSize(2u));

  fake_dlcservice_client_->SetGetInstalledError(dlcservice::kErrorInternal);
  EXPECT_EQ(CallGetDlcListAndReturnSize(), 0u);

  fake_dlcservice_client_->SetGetInstalledError(dlcservice::kErrorNeedReboot);
  EXPECT_EQ(CallGetDlcListAndReturnSize(), 0u);

  fake_dlcservice_client_->SetGetInstalledError(dlcservice::kErrorInvalidDlc);
  EXPECT_EQ(CallGetDlcListAndReturnSize(), 0u);

  fake_dlcservice_client_->SetGetInstalledError(dlcservice::kErrorAllocation);
  EXPECT_EQ(CallGetDlcListAndReturnSize(), 0u);

  fake_dlcservice_client_->SetGetInstalledError(dlcservice::kErrorNone);
  EXPECT_EQ(CallGetDlcListAndReturnSize(), 2u);
}

}  // namespace

}  // namespace settings
}  // namespace chromeos
