// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/token.h"
#include "base/values.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_service_manager_listener.h"
#include "content/public/browser/system_connector.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "services/data_decoder/public/cpp/safe_json_parser.h"
#include "services/data_decoder/public/mojom/constants.mojom.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"

namespace {

using data_decoder::SafeJsonParser;

constexpr char kTestJson[] = "[\"awesome\", \"possum\"]";

std::string MaybeToJson(const base::Value& value) {
  std::string json;
  if (!base::JSONWriter::Write(value, &json))
    return "(invalid value)";

  return json;
}

class SafeJsonParserTest : public InProcessBrowserTest {
 public:
  SafeJsonParserTest() = default;

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    listener_.Init();
  }

  // Tests SafeJsonParser::Parse/ParseBatch. Parses |json| using SafeJsonParser
  // and verifies that the correct callbacks are called. If |batch_id| is not
  // empty, uses SafeJsonParser::ParseBatch to batch multiple parse requests.
  void Parse(const std::string& json,
             const base::Optional<base::Token>& batch_id = base::nullopt) {
    SCOPED_TRACE(json);
    DCHECK(!message_loop_runner_);
    message_loop_runner_ = new content::MessageLoopRunner;

    base::JSONReader::ValueWithError value_with_error =
        base::JSONReader::ReadAndReturnValueWithError(json,
                                                      base::JSON_PARSE_RFC);

    SafeJsonParser::SuccessCallback success_callback;
    SafeJsonParser::ErrorCallback error_callback;
    if (value_with_error.value) {
      success_callback = base::BindOnce(&SafeJsonParserTest::ExpectValue,
                                        base::Unretained(this),
                                        std::move(*value_with_error.value));
      error_callback = base::BindOnce(&SafeJsonParserTest::FailWithError,
                                      base::Unretained(this));
    } else {
      success_callback = base::BindOnce(&SafeJsonParserTest::FailWithValue,
                                        base::Unretained(this));
      error_callback = base::BindOnce(&SafeJsonParserTest::ExpectError,
                                      base::Unretained(this),
                                      value_with_error.error_message);
    }

    if (batch_id) {
      SafeJsonParser::ParseBatch(content::GetSystemConnector(), json,
                                 std::move(success_callback),
                                 std::move(error_callback), *batch_id);
    } else {
      SafeJsonParser::Parse(content::GetSystemConnector(), json,
                            std::move(success_callback),
                            std::move(error_callback));
    }

    message_loop_runner_->Run();
    message_loop_runner_ = nullptr;
  }

  uint32_t GetServiceStartCount(const std::string& service_name) const {
    return listener_.GetServiceStartCount(service_name);
  }

 private:
  void ExpectValue(base::Value expected_value, base::Value actual_value) {
    EXPECT_EQ(expected_value, actual_value)
        << "Expected: " << MaybeToJson(expected_value)
        << " Actual: " << MaybeToJson(actual_value);
    message_loop_runner_->Quit();
  }

  void ExpectError(const std::string& expected_error,
                   const std::string& actual_error) {
    EXPECT_EQ(expected_error, actual_error);
    message_loop_runner_->Quit();
  }

  void FailWithValue(base::Value value) {
    ADD_FAILURE() << MaybeToJson(value);
    message_loop_runner_->Quit();
  }

  void FailWithError(const std::string& error) {
    ADD_FAILURE() << error;
    message_loop_runner_->Quit();
  }

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  TestServiceManagerListener listener_;

  DISALLOW_COPY_AND_ASSIGN(SafeJsonParserTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SafeJsonParserTest, Parse) {
  Parse("{}");
  Parse("choke");
  Parse("{\"awesome\": true}");
  Parse("\"laser\"");
  Parse("false");
  Parse("null");
  Parse("3.14");
  Parse("[");
  Parse("\"");
  Parse(std::string());
  Parse("☃");
  Parse("\"☃\"");
  Parse("\"\\ufdd0\"");
  Parse("\"\\ufffe\"");
  Parse("\"\\ud83f\\udffe\"");
}

// Tests that when calling SafeJsonParser::Parse() a new service is started
// every time.
IN_PROC_BROWSER_TEST_F(SafeJsonParserTest, Isolation) {
  for (int i = 0; i < 5; i++) {
    SCOPED_TRACE(base::StringPrintf("Testing iteration %d", i));
    Parse(kTestJson);
    EXPECT_EQ(1U + i, GetServiceStartCount(data_decoder::mojom::kServiceName));
  }
}

// Tests that using a batch ID allows service reuse.
IN_PROC_BROWSER_TEST_F(SafeJsonParserTest, IsolationWithGroups) {
  constexpr base::Token kBatchId1{0, 1};
  constexpr base::Token kBatchId2{0, 2};
  for (int i = 0; i < 5; i++) {
    SCOPED_TRACE(base::StringPrintf("Testing iteration %d", i));
    Parse(kTestJson, kBatchId1);
    Parse(kTestJson, kBatchId2);
  }
  EXPECT_EQ(2U, GetServiceStartCount(data_decoder::mojom::kServiceName));
}
