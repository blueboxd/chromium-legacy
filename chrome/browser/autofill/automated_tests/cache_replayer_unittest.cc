// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/automated_tests/cache_replayer.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace autofill {
namespace test {
namespace {

// Only run these tests on Linux because there are issues with other platforms.
// Testing on one platform gives enough confidence.
#if defined(OS_LINUX)

using base::JSONWriter;
using base::Value;

using RequestResponsePair =
    std::pair<AutofillQueryContents, AutofillQueryResponseContents>;

constexpr char kTestHTTPRequestHeader[] = "Fake HTTP Request Header";
constexpr char kTestHTTPResponseHeader[] = "Fake HTTP Response Header";
constexpr char kHTTPBodySep[] = "\r\n\r\n";

struct LightField {
  uint32_t signature;
  uint32_t prediction;
};

struct LightForm {
  uint64_t signature;
  std::vector<LightField> fields;
};

RequestResponsePair MakeQueryRequestResponsePair(
    const std::vector<LightForm>& forms) {
  AutofillQueryContents query;
  query.set_client_version("Chrome Test");
  AutofillQueryResponseContents query_response;
  for (const auto& form : forms) {
    auto* added_form = query.add_form();
    added_form->set_signature(form.signature);
    for (const auto& field : form.fields) {
      added_form->add_field()->set_signature(field.signature);
      query_response.add_field()->set_overall_type_prediction(field.prediction);
    }
  }
  return RequestResponsePair({std::move(query), std::move(query_response)});
}

std::string MakeSerializedRequest(const AutofillQueryContents& query) {
  std::string serialized_query;
  query.SerializeToString(&serialized_query);
  // TODO(vincb): Put a real header here.
  std::string http_text = base::JoinString(
      std::vector<std::string>{kTestHTTPRequestHeader, serialized_query},
      kHTTPBodySep);
  std::string encoded_http_text;
  base::Base64Encode(http_text, &encoded_http_text);
  return encoded_http_text;
}

std::string MakeSerializedResponse(
    const AutofillQueryResponseContents& query_response) {
  std::string serialized_query;
  query_response.SerializeToString(&serialized_query);
  std::string compressed_query;
  compression::GzipCompress(serialized_query, &compressed_query);
  // TODO(vincb): Put a real header here.
  std::string http_text = base::JoinString(
      std::vector<std::string>{kTestHTTPResponseHeader, compressed_query},
      kHTTPBodySep);
  std::string encoded_http_text;
  base::Base64Encode(http_text, &encoded_http_text);
  return encoded_http_text;
}

bool WriteJSON(const base::FilePath& file_path,
               const std::vector<RequestResponsePair>& request_response_pairs) {
  // Make json list node that contains all query requests.
  Value::ListStorage autofill_requests;
  for (const auto& request_response_pair : request_response_pairs) {
    Value::DictStorage request_response_node;
    request_response_node["SerializedRequest"] = std::make_unique<Value>(
        MakeSerializedRequest(request_response_pair.first));
    request_response_node["SerializedResponse"] = std::make_unique<Value>(
        MakeSerializedResponse(request_response_pair.second));
    autofill_requests.push_back(Value(std::move(request_response_node)));
  }

  // Make json dict node that contains Autofill Server requests per URL.
  base::Value::DictStorage urls_dict;
  urls_dict["https://clients1.google.com/tbproxy/af/query?"] =
      std::make_unique<Value>(std::move(autofill_requests));

  // Make json dict node that contains requests per domain.
  base::Value::DictStorage domains_dict;
  domains_dict["clients1.google.com"] =
      std::make_unique<Value>(std::move(urls_dict));

  // Make json root dict.
  base::Value::DictStorage root_dict;
  root_dict["Requests"] = std::make_unique<Value>(std::move(domains_dict));

  std::string json_text;
  JSONWriter::WriteWithOptions(Value(std::move(root_dict)),
                               JSONWriter::Options::OPTIONS_PRETTY_PRINT,
                               &json_text);

  std::string compressed_json_text;
  if (!compression::GzipCompress(json_text, &compressed_json_text)) {
    VLOG(1) << "Cannot compress json to gzip.";
    return false;
  }

  if (base::WriteFile(file_path, compressed_json_text.data(),
                      compressed_json_text.size()) == -1) {
    VLOG(1) << "Could not write json at file: " << file_path;
    return false;
  }
  return true;
}

// TODO(vincb): Add extra death tests.
TEST(AutofillCacheReplayerDeathTest,
     ServerCacheReplayerConstructor_CrashesWhenNoDomainNode) {
  // Make death test threadsafe.
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  // Make writable file path.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("test_wpr_capture.json");

  // JSON structure is not right.
  const std::string invalid_json = "{\"NoDomainNode\": \"invalid_field\"}";

  // Write json to file.
  ASSERT_TRUE(
      base::WriteFile(file_path, invalid_json.data(), invalid_json.size()) > -1)
      << "there was an error when writing content to json file: " << file_path;

  // Crash since json content is invalid.
  ASSERT_DEATH_IF_SUPPORTED(
      ServerCacheReplayer(file_path,
                          ServerCacheReplayer::kOptionFailOnInvalidJsonRecord),
      ".*");
}

TEST(AutofillCacheReplayerDeathTest,
     ServerCacheReplayerConstructor_CrashesWhenNoQueryNodesAndFailOnEmpty) {
  // Make death test threadsafe.
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  // Make writable file path.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("test_wpr_capture.json");

  // Make empty request/response pairs to write in cache.
  std::vector<RequestResponsePair> request_response_pairs;

  // Write cache to json and create replayer.
  ASSERT_TRUE(WriteJSON(file_path, request_response_pairs));

  // Crash since there are no Query nodes and set to fail on empty.
  ASSERT_DEATH_IF_SUPPORTED(
      ServerCacheReplayer(file_path,
                          ServerCacheReplayer::kOptionFailOnInvalidJsonRecord |
                              ServerCacheReplayer::kOptionFailOnEmpty),
      ".*");
}

TEST(AutofillCacheReplayerDeathTest,
     CanUseReplayerWhenNoCacheContentWithNotFailOnEmpty) {
  // Make death test threadsafe.
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  // Make writable file path.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("test_wpr_capture.json");

  // Make empty request/response pairs to write in cache.
  std::vector<RequestResponsePair> request_response_pairs;

  // Write cache to json and create replayer.
  ASSERT_TRUE(WriteJSON(file_path, request_response_pairs));

  // Should not crash even if no cache because kOptionFailOnEmpty is not
  // flipped.
  ServerCacheReplayer cache_replayer(
      file_path, ServerCacheReplayer::kOptionFailOnInvalidJsonRecord &
                     (ServerCacheReplayer::kOptionFailOnEmpty & 0));

  // Should be able to read cache, which will give nothing.
  std::string http_text;
  AutofillQueryContents query_with_no_match;
  EXPECT_FALSE(
      cache_replayer.GetResponseForQuery(query_with_no_match, &http_text));
}

TEST(AutofillCacheReplayerTest, GetResponseForQueryFillsResponseWhenNoErrors) {
  // Make writable file path.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("test_wpr_capture.json");

  // Make request/response pairs to write in cache.
  std::vector<RequestResponsePair> request_response_pairs;
  {
    LightForm form_to_add;
    form_to_add.signature = 1234;
    form_to_add.fields = {LightField{1234, 1}};
    request_response_pairs.push_back(
        MakeQueryRequestResponsePair({form_to_add}));
  }

  // Write cache to json.
  ASSERT_TRUE(WriteJSON(file_path, request_response_pairs));

  ServerCacheReplayer cache_replayer(
      file_path, ServerCacheReplayer::kOptionFailOnInvalidJsonRecord &
                     ServerCacheReplayer::kOptionFailOnEmpty);

  // Verify if we can get cached response.
  std::string http_text_response;
  ASSERT_TRUE(cache_replayer.GetResponseForQuery(
      request_response_pairs[0].first, &http_text_response));
  AutofillQueryResponseContents response_from_cache;
  ASSERT_TRUE(response_from_cache.ParseFromString(
      SplitHTTP(http_text_response).second));
}

TEST(AutofillCacheReplayerTest, GetResponseForQueryGivesFalseWhenNullptr) {
  ServerCacheReplayer cache_replayer(ServerCache{{}});
  EXPECT_FALSE(
      cache_replayer.GetResponseForQuery(AutofillQueryContents(), nullptr));
}

TEST(AutofillCacheReplayerTest, GetResponseForQueryGivesFalseWhenNoKeyMatch) {
  // Make writable file path.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("test_wpr_capture.json");

  // Make request/response pairs to write in cache.
  std::vector<RequestResponsePair> request_response_pairs;
  {
    LightForm form_to_add;
    form_to_add.signature = 1234;
    form_to_add.fields = {LightField{1234, 1}};
    request_response_pairs.push_back(
        MakeQueryRequestResponsePair({form_to_add}));
  }

  // Write cache to json and create replayer.
  ASSERT_TRUE(WriteJSON(file_path, request_response_pairs));
  ServerCacheReplayer cache_replayer(
      file_path, ServerCacheReplayer::kOptionFailOnInvalidJsonRecord &
                     ServerCacheReplayer::kOptionFailOnEmpty);

  // Verify if we get false when there is no cache for the query.
  std::string http_text;
  AutofillQueryContents query_with_no_match;
  EXPECT_FALSE(
      cache_replayer.GetResponseForQuery(query_with_no_match, &http_text));
}

TEST(AutofillCacheReplayerTest,
     GetResponseForQueryGivesFalseWhenDecompressFailsBecauseInvalidHTTP) {
  // Make query request and key.
  LightForm form_to_add;
  form_to_add.signature = 1234;
  form_to_add.fields = {LightField{1234, 1}};
  const AutofillQueryContents query_request_for_key =
      MakeQueryRequestResponsePair({form_to_add}).first;
  const std::string key = GetKeyFromQueryRequest(query_request_for_key);

  const char invalid_http[] = "";
  ServerCacheReplayer cache_replayer(ServerCache{{key, invalid_http}});

  // Verify if we get false when invalid HTTP response to decompress.
  std::string response_http_text;
  EXPECT_FALSE(cache_replayer.GetResponseForQuery(query_request_for_key,
                                                  &response_http_text));
}

TEST(AutofillCacheReplayerTest,
     GetResponseForQueryGivesTrueWhenDecompressSucceededBecauseEmptyBody) {
  // Make query request and key.
  LightForm form_to_add;
  form_to_add.signature = 1234;
  form_to_add.fields = {LightField{1234, 1}};
  const AutofillQueryContents query_request_for_key =
      MakeQueryRequestResponsePair({form_to_add}).first;
  const std::string key = GetKeyFromQueryRequest(query_request_for_key);

  const char http_without_body[] = "Test HTTP Header\r\n\r\n";
  ServerCacheReplayer cache_replayer(ServerCache{{key, http_without_body}});

  // Verify if we get true when no HTTP body.
  std::string response_http_text;
  EXPECT_TRUE(cache_replayer.GetResponseForQuery(query_request_for_key,
                                                 &response_http_text));
}
#endif  // if defined(OS_LINUX)

}  // namespace
}  // namespace test
}  // namespace autofill
