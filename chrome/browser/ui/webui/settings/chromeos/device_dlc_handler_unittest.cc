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

dlcservice::DlcsWithContent CreateDlcModuleListOfSize(size_t numDlcs) {
  dlcservice::DlcsWithContent dlc_module_list;
  dlcservice::DlcsWithContent_DlcInfo* dlc_module_info;
  for (size_t i = 0; i < numDlcs; i++) {
    dlc_module_info = dlc_module_list.add_dlc_infos();
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

  base::Value::ConstListView CallGetDlcListAndReturnList() {
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
    return call_data.arg3()->GetList();
  }

  bool CallPurgeDlcAndReturnSuccess() {
    size_t call_data_count_before_call = test_web_ui()->call_data().size();

    base::ListValue args;
    args.AppendString("handlerFunctionName");
    args.AppendString("dlcId");
    test_web_ui()->HandleReceivedMessage("purgeDlc", &args);
    task_environment_.RunUntilIdle();

    EXPECT_EQ(call_data_count_before_call + 1u,
              test_web_ui()->call_data().size());

    const content::TestWebUI::CallData& call_data =
        CallDataAtIndex(call_data_count_before_call);
    EXPECT_EQ("cr.webUIResponse", call_data.function_name());
    EXPECT_EQ("handlerFunctionName", call_data.arg1()->GetString());
    return call_data.arg3()->GetBool();
  }
};

TEST_F(DlcHandlerTest, GetDlcList) {
  fake_dlcservice_client_->set_dlcs_with_content(CreateDlcModuleListOfSize(2u));

  fake_dlcservice_client_->SetGetExistingDlcsError(dlcservice::kErrorInternal);
  EXPECT_EQ(CallGetDlcListAndReturnList().size(), 0u);

  fake_dlcservice_client_->SetGetExistingDlcsError(
      dlcservice::kErrorNeedReboot);
  EXPECT_EQ(CallGetDlcListAndReturnList().size(), 0u);

  fake_dlcservice_client_->SetGetExistingDlcsError(
      dlcservice::kErrorInvalidDlc);
  EXPECT_EQ(CallGetDlcListAndReturnList().size(), 0u);

  fake_dlcservice_client_->SetGetExistingDlcsError(
      dlcservice::kErrorAllocation);
  EXPECT_EQ(CallGetDlcListAndReturnList().size(), 0u);

  fake_dlcservice_client_->SetGetExistingDlcsError(dlcservice::kErrorNone);
  EXPECT_EQ(CallGetDlcListAndReturnList().size(), 2u);
}

TEST_F(DlcHandlerTest, PurgeDlc) {
  fake_dlcservice_client_->SetPurgeError(dlcservice::kErrorInternal);
  EXPECT_FALSE(CallPurgeDlcAndReturnSuccess());

  fake_dlcservice_client_->SetPurgeError(dlcservice::kErrorNeedReboot);
  EXPECT_FALSE(CallPurgeDlcAndReturnSuccess());

  fake_dlcservice_client_->SetPurgeError(dlcservice::kErrorInvalidDlc);
  EXPECT_FALSE(CallPurgeDlcAndReturnSuccess());

  fake_dlcservice_client_->SetPurgeError(dlcservice::kErrorAllocation);
  EXPECT_FALSE(CallPurgeDlcAndReturnSuccess());

  fake_dlcservice_client_->SetPurgeError(dlcservice::kErrorNone);
  EXPECT_TRUE(CallPurgeDlcAndReturnSuccess());
}

TEST_F(DlcHandlerTest, FormattedCorrectly) {
  dlcservice::DlcsWithContent dlcs_with_content;
  auto* dlc_info = dlcs_with_content.add_dlc_infos();
  dlc_info->set_id("fake id");
  dlc_info->set_name("fake name");
  dlc_info->set_description("fake description");
  dlc_info->set_used_bytes_on_disk(100000);

  fake_dlcservice_client_->set_dlcs_with_content(dlcs_with_content);

  auto result_list = CallGetDlcListAndReturnList();
  EXPECT_EQ(1UL, result_list.size());
  EXPECT_EQ("fake id", result_list[0].FindKey("id")->GetString());
  EXPECT_EQ("fake name", result_list[0].FindKey("name")->GetString());
  EXPECT_EQ("fake description",
            result_list[0].FindKey("description")->GetString());
  EXPECT_EQ("97.7 KB", result_list[0].FindKey("diskUsageLabel")->GetString());
}

}  // namespace

}  // namespace settings
}  // namespace chromeos
