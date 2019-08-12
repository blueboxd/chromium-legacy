// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/ime_service.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/services/ime/public/mojom/constants.mojom.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace chromeos {
namespace ime {

namespace {

const char kInvalidImeSpec[] = "ime_spec_never_support";
const std::vector<uint8_t> extra{0x66, 0x77, 0x88};

void ConnectCallback(bool* success, bool result) {
  *success = result;
}

void TestProcessTextCallback(std::string* res_out,
                             const std::string& response) {
  *res_out = response;
}

void TestProcessKeypressForRulebasedCallback(
    mojom::KeypressResponseForRulebased* res_out,
    mojom::KeypressResponseForRulebasedPtr response) {
  res_out->result = response->result;
  res_out->operations = std::vector<mojom::OperationForRulebasedPtr>(0);
  for (int i = 0; i < (int)response->operations.size(); i++) {
    res_out->operations.push_back(std::move(response->operations[i]));
  }
}
void TestGetRulebasedKeypressCountForTestingCallback(int32_t* res_out,
                                                     int32_t response) {
  *res_out = response;
}

class TestClientChannel : mojom::InputChannel {
 public:
  TestClientChannel() : receiver_(this) {}

  mojo::PendingRemote<mojom::InputChannel> CreatePendingRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // mojom::InputChannel implementation.
  MOCK_METHOD2(ProcessText, void(const std::string&, ProcessTextCallback));
  MOCK_METHOD2(ProcessMessage,
               void(const std::vector<uint8_t>& message,
                    ProcessMessageCallback));
  MOCK_METHOD2(ProcessKeypressForRulebased,
               void(const mojom::KeypressInfoForRulebasedPtr message,
                    ProcessKeypressForRulebasedCallback));
  MOCK_METHOD0(ResetForRulebased, void());
  MOCK_METHOD1(GetRulebasedKeypressCountForTesting,
               void(GetRulebasedKeypressCountForTestingCallback));

 private:
  mojo::Receiver<mojom::InputChannel> receiver_;

  DISALLOW_COPY_AND_ASSIGN(TestClientChannel);
};

class ImeServiceTest : public testing::Test {
 public:
  ImeServiceTest()
      : service_(
            test_connector_factory_.RegisterInstance(mojom::kServiceName)) {}
  ~ImeServiceTest() override = default;

  MOCK_METHOD1(SentTextCallback, void(const std::string&));
  MOCK_METHOD1(SentMessageCallback, void(const std::vector<uint8_t>&));

 protected:
  void SetUp() override {
    test_connector_factory_.GetDefaultConnector()->Connect(
        mojom::kServiceName, remote_manager_.BindNewPipeAndPassReceiver());
  }

  mojo::Remote<mojom::InputEngineManager> remote_manager_;

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  service_manager::TestConnectorFactory test_connector_factory_;
  ImeService service_;

  DISALLOW_COPY_AND_ASSIGN(ImeServiceTest);
};

}  // namespace

// Tests that the service is instantiated and it will return false when
// activating an IME engine with an invalid IME spec.
TEST_F(ImeServiceTest, ConnectInvalidImeEngine) {
  bool success = true;
  TestClientChannel test_channel;
  mojo::Remote<mojom::InputChannel> remote_engine;

  remote_manager_->ConnectToImeEngine(
      kInvalidImeSpec, remote_engine.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_FALSE(success);
}

TEST_F(ImeServiceTest, MultipleClients) {
  bool success = false;
  TestClientChannel test_channel_1;
  TestClientChannel test_channel_2;
  mojo::Remote<mojom::InputChannel> remote_engine_1;
  mojo::Remote<mojom::InputChannel> remote_engine_2;

  remote_manager_->ConnectToImeEngine(
      "m17n:ar", remote_engine_1.BindNewPipeAndPassReceiver(),
      test_channel_1.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();

  remote_manager_->ConnectToImeEngine(
      "m17n:ar", remote_engine_2.BindNewPipeAndPassReceiver(),
      test_channel_2.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();

  std::string response;
  std::string process_text_key =
      "{\"method\":\"keyEvent\",\"type\":\"keydown\""
      ",\"code\":\"KeyA\",\"shift\":true,\"altgr\":false,\"caps\":false}";
  remote_engine_1->ProcessText(
      process_text_key, base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine_1.FlushForTesting();

  remote_engine_2->ProcessText(
      process_text_key, base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine_2.FlushForTesting();

  std::string process_text_key_count = "{\"method\":\"countKey\"}";
  remote_engine_1->ProcessText(
      process_text_key_count,
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine_1.FlushForTesting();
  EXPECT_EQ("1", response);

  remote_engine_2->ProcessText(
      process_text_key_count,
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine_2.FlushForTesting();
  EXPECT_EQ("1", response);
}

// Tests that the rule-based Arabic keyboard can work correctly.
TEST_F(ImeServiceTest, RuleBasedArabic) {
  bool success = false;
  TestClientChannel test_channel;
  mojo::Remote<mojom::InputChannel> remote_engine;

  remote_manager_->ConnectToImeEngine(
      "m17n:ar", remote_engine.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  // Test Shift+KeyA.
  std::string response;
  remote_engine->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"KeyA\","
      "\"shift\":true,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine.FlushForTesting();
  const wchar_t* expected_response =
      L"{\"result\":true,\"operations\":[{\"method\":\"commitText\","
      L"\"arguments\":[\"\u0650\"]}]}";
  EXPECT_EQ(base::WideToUTF8(expected_response), response);

  // Test KeyB.
  remote_engine->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"KeyB\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine.FlushForTesting();
  expected_response =
      L"{\"result\":true,\"operations\":[{\"method\":\"commitText\","
      L"\"arguments\":[\"\u0644\u0627\"]}]}";
  EXPECT_EQ(base::WideToUTF8(expected_response), response);

  // Test unhandled key.
  remote_engine->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"Enter\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine.FlushForTesting();
  EXPECT_EQ("{\"result\":false}", response);

  // Test keyup.
  remote_engine->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keyup\",\"code\":\"Enter\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine.FlushForTesting();
  EXPECT_EQ("{\"result\":false}", response);

  // Test reset.
  remote_engine->ProcessText(
      "{\"method\":\"reset\"}",
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine.FlushForTesting();
  EXPECT_EQ("{\"result\":true}", response);

  // Test invalid request.
  remote_engine->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\"}",
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine.FlushForTesting();
  EXPECT_EQ("{\"result\":false}", response);
}

// Tests that the rule-based DevaPhone keyboard can work correctly.
TEST_F(ImeServiceTest, RuleBasedDevaPhone) {
  bool success = false;
  TestClientChannel test_channel;
  mojo::Remote<mojom::InputChannel> remote_engine;

  remote_manager_->ConnectToImeEngine(
      "m17n:deva_phone", remote_engine.BindNewPipeAndPassReceiver(),
      test_channel.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  std::string response;

  // KeyN.
  remote_engine->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"KeyN\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine.FlushForTesting();
  const char* expected_response =
      u8"{\"result\":true,\"operations\":[{\"method\":\"setComposition\","
      u8"\"arguments\":[\"\u0928\"]}]}";
  EXPECT_EQ(expected_response, response);

  // Backspace.
  remote_engine->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"Backspace\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine.FlushForTesting();
  expected_response =
      u8"{\"result\":true,\"operations\":[{\"method\":\"setComposition\","
      u8"\"arguments\":[\"\"]}]}";
  EXPECT_EQ(expected_response, response);

  // KeyN + KeyC.
  remote_engine->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"KeyN\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"KeyC\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine.FlushForTesting();
  expected_response =
      u8"{\"result\":true,\"operations\":[{\"method\":\"setComposition\","
      u8"\"arguments\":[\"\u091e\u094d\u091a\"]}]}";
  EXPECT_EQ(expected_response, response);

  // Space.
  remote_engine->ProcessText(
      "{\"method\":\"keyEvent\",\"type\":\"keydown\",\"code\":\"Space\","
      "\"shift\":false,\"altgr\":false,\"caps\":false}",
      base::BindOnce(&TestProcessTextCallback, &response));
  remote_engine.FlushForTesting();
  expected_response =
      u8"{\"result\":true,\"operations\":[{\"method\":\"commitText\","
      u8"\"arguments\":[\"\u091e\u094d\u091a \"]}]}";
  EXPECT_EQ(expected_response, response);
}

TEST_F(ImeServiceTest, MultipleClientsRulebased) {
  bool success = false;
  TestClientChannel test_channel_1;
  TestClientChannel test_channel_2;
  mojo::Remote<mojom::InputChannel> remote_engine_1;
  mojo::Remote<mojom::InputChannel> remote_engine_2;

  remote_manager_->ConnectToImeEngine(
      "m17n:ar", remote_engine_1.BindNewPipeAndPassReceiver(),
      test_channel_1.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();

  remote_manager_->ConnectToImeEngine(
      "m17n:ar", remote_engine_2.BindNewPipeAndPassReceiver(),
      test_channel_2.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();

  mojom::KeypressResponseForRulebased response;
  mojom::KeypressInfoForRulebasedPtr keypress_info =
      mojom::KeypressInfoForRulebased::New("keydown", "KeyA", true, false,
                                           false, false, false);
  remote_engine_1->ProcessKeypressForRulebased(
      mojom::KeypressInfoForRulebased::New("keydown", "KeyA", true, false,
                                           false, false, false),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  remote_engine_1.FlushForTesting();

  remote_engine_2->ProcessKeypressForRulebased(
      mojom::KeypressInfoForRulebased::New("keydown", "KeyA", true, false,
                                           false, false, false),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  remote_engine_2.FlushForTesting();

  int32_t count;
  remote_engine_1->GetRulebasedKeypressCountForTesting(
      base::BindOnce(&TestGetRulebasedKeypressCountForTestingCallback, &count));
  remote_engine_1.FlushForTesting();
  EXPECT_EQ(1, count);

  remote_engine_2->GetRulebasedKeypressCountForTesting(
      base::BindOnce(&TestGetRulebasedKeypressCountForTestingCallback, &count));
  remote_engine_2.FlushForTesting();
  EXPECT_EQ(1, count);
}

// Tests that the rule-based Arabic keyboard can work correctly.
TEST_F(ImeServiceTest, RuleBasedArabicKeypress) {
  bool success = false;
  TestClientChannel test_channel;
  mojom::InputChannelPtr to_engine_ptr;

  remote_manager_->ConnectToImeEngine(
      "m17n:ar", mojo::MakeRequest(&to_engine_ptr),
      test_channel.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  // Test Shift+KeyA.
  mojom::KeypressResponseForRulebased response;
  to_engine_ptr->ProcessKeypressForRulebased(
      mojom::KeypressInfoForRulebased::New("keydown", "KeyA", true, false,
                                           false, false, false),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_ptr.FlushForTesting();

  EXPECT_EQ(response.result, true);
  std::vector<mojom::OperationForRulebasedPtr> expected_operations;
  expected_operations.push_back({mojom::OperationForRulebased::New(
      mojom::OperationMethodForRulebased::COMMIT_TEXT, "\u0650")});
  EXPECT_EQ(response.operations.size(), expected_operations.size());
  EXPECT_EQ(response.operations, expected_operations);

  // Test KeyB
  to_engine_ptr->ProcessKeypressForRulebased(
      mojom::KeypressInfoForRulebased::New("keydown", "KeyB", false, false,
                                           false, false, false),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_ptr.FlushForTesting();
  EXPECT_EQ(response.result, true);
  expected_operations = std::vector<mojom::OperationForRulebasedPtr>(0);
  expected_operations.push_back({mojom::OperationForRulebased::New(
      mojom::OperationMethodForRulebased::COMMIT_TEXT, "\u0644\u0627")});
  EXPECT_EQ(response.operations.size(), expected_operations.size());
  EXPECT_EQ(response.operations, expected_operations);

  // Test unhandled key.
  to_engine_ptr->ProcessKeypressForRulebased(
      mojom::KeypressInfoForRulebased::New("keydown", "Enter", false, false,
                                           false, false, false),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_ptr.FlushForTesting();
  EXPECT_EQ(response.result, false);

  // Test keyup.
  to_engine_ptr->ProcessKeypressForRulebased(
      mojom::KeypressInfoForRulebased::New("keyup", "Enter", false, false,
                                           false, false, false),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_ptr.FlushForTesting();
  EXPECT_EQ(response.result, false);

  // TODO(keithlee) Test reset function
  to_engine_ptr->ResetForRulebased();

  // Test invalid request.
  to_engine_ptr->ProcessKeypressForRulebased(
      mojom::KeypressInfoForRulebased::New("keydown", "", false, false, false,
                                           false, false),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_ptr.FlushForTesting();
  EXPECT_EQ(response.result, false);
}

// Tests that the rule-based DevaPhone keyboard can work correctly.
TEST_F(ImeServiceTest, RuleBasedDevaPhoneKeypress) {
  bool success = false;
  TestClientChannel test_channel;
  mojom::InputChannelPtr to_engine_ptr;

  remote_manager_->ConnectToImeEngine(
      "m17n:deva_phone", mojo::MakeRequest(&to_engine_ptr),
      test_channel.CreatePendingRemote(), extra,
      base::BindOnce(&ConnectCallback, &success));
  remote_manager_.FlushForTesting();
  EXPECT_TRUE(success);

  mojom::KeypressResponseForRulebased response;
  std::vector<mojom::OperationForRulebasedPtr> expected_operations;

  // Test KeyN.
  to_engine_ptr->ProcessKeypressForRulebased(
      mojom::KeypressInfoForRulebased::New("keydown", "KeyN", false, false,
                                           false, false, false),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_ptr.FlushForTesting();

  EXPECT_EQ(response.result, true);
  expected_operations = std::vector<mojom::OperationForRulebasedPtr>(0);
  expected_operations.push_back({mojom::OperationForRulebased::New(
      mojom::OperationMethodForRulebased::SET_COMPOSITION, "\u0928")});
  EXPECT_EQ(response.operations.size(), expected_operations.size());
  EXPECT_EQ(response.operations, expected_operations);

  // Backspace.
  to_engine_ptr->ProcessKeypressForRulebased(
      mojom::KeypressInfoForRulebased::New("keydown", "Backspace", false, false,
                                           false, false, false),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_ptr.FlushForTesting();

  EXPECT_EQ(response.result, true);
  expected_operations = std::vector<mojom::OperationForRulebasedPtr>(0);
  expected_operations.push_back({mojom::OperationForRulebased::New(
      mojom::OperationMethodForRulebased::SET_COMPOSITION, "")});
  EXPECT_EQ(response.operations.size(), expected_operations.size());
  EXPECT_EQ(response.operations, expected_operations);

  // KeyN + KeyC.
  to_engine_ptr->ProcessKeypressForRulebased(
      mojom::KeypressInfoForRulebased::New("keydown", "KeyN", false, false,
                                           false, false, false),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_ptr->ProcessKeypressForRulebased(
      mojom::KeypressInfoForRulebased::New("keydown", "KeyC", false, false,
                                           false, false, false),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_ptr.FlushForTesting();

  EXPECT_EQ(response.result, true);
  expected_operations = std::vector<mojom::OperationForRulebasedPtr>(0);
  expected_operations.push_back({mojom::OperationForRulebased::New(
      mojom::OperationMethodForRulebased::SET_COMPOSITION,
      "\u091e\u094d\u091a")});
  EXPECT_EQ(response.operations.size(), expected_operations.size());
  EXPECT_EQ(response.operations, expected_operations);

  // Space.
  to_engine_ptr->ProcessKeypressForRulebased(
      mojom::KeypressInfoForRulebased::New("keydown", "Space", false, false,
                                           false, false, false),
      base::BindOnce(&TestProcessKeypressForRulebasedCallback, &response));
  to_engine_ptr.FlushForTesting();

  EXPECT_EQ(response.result, true);
  expected_operations = std::vector<mojom::OperationForRulebasedPtr>(0);
  expected_operations.push_back({mojom::OperationForRulebased::New(
      mojom::OperationMethodForRulebased::COMMIT_TEXT, "\u091e\u094d\u091a ")});
  EXPECT_EQ(response.operations.size(), expected_operations.size());
  EXPECT_EQ(response.operations, expected_operations);
}

}  // namespace ime
}  // namespace chromeos
