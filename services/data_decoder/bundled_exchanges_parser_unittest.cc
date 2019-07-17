// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/bundled_exchanges_parser.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "components/cbor/writer.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_decoder {

namespace {

std::string GetTestFileContents(const base::FilePath& path) {
  base::FilePath test_data_dir;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir);
  test_data_dir = test_data_dir.Append(base::FilePath(
      FILE_PATH_LITERAL("services/test/data/bundled_exchanges")));

  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(test_data_dir.Append(path), &contents));
  return contents;
}

class TestDataSource : public mojom::BundleDataSource {
 public:
  explicit TestDataSource(const base::FilePath& path)
      : data_(GetTestFileContents(path)) {}
  explicit TestDataSource(const std::vector<uint8_t>& data)
      : data_(reinterpret_cast<const char*>(data.data()), data.size()) {}

  void GetSize(GetSizeCallback callback) override {
    std::move(callback).Run(data_.size());
  }

  void Read(uint64_t offset, uint64_t length, ReadCallback callback) override {
    if (offset + length > data_.size())
      std::move(callback).Run(base::nullopt);
    const uint8_t* start =
        reinterpret_cast<const uint8_t*>(data_.data()) + offset;
    std::move(callback).Run(std::vector<uint8_t>(start, start + length));
  }

  base::StringPiece GetPayload(const mojom::BundleResponsePtr& response) {
    return base::StringPiece(data_).substr(response->payload_offset,
                                           response->payload_length);
  }

  void AddReceiver(mojo::PendingReceiver<mojom::BundleDataSource> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

 private:
  std::string data_;
  mojo::ReceiverSet<mojom::BundleDataSource> receivers_;
};

mojom::BundleMetadataPtr ParseBundle(TestDataSource* data_source) {
  mojo::PendingRemote<mojom::BundleDataSource> source_remote;
  data_source->AddReceiver(source_remote.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<mojom::BundledExchangesParser> parser_remote;
  BundledExchangesParser parser_impl(
      parser_remote.InitWithNewPipeAndPassReceiver(), std::move(source_remote));
  data_decoder::mojom::BundledExchangesParser& parser = parser_impl;

  base::RunLoop run_loop;
  mojom::BundleMetadataPtr result;
  parser.ParseMetadata(base::BindLambdaForTesting(
      [&result, &run_loop](mojom::BundleMetadataPtr metadata,
                           const base::Optional<std::string>& error) {
        result = std::move(metadata);
        run_loop.QuitClosure().Run();
      }));
  run_loop.Run();
  return result;
}

mojom::BundleResponsePtr ParseResponse(TestDataSource* data_source,
                                       const mojom::BundleIndexItemPtr& item) {
  mojo::PendingRemote<mojom::BundleDataSource> source_remote;
  data_source->AddReceiver(source_remote.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<mojom::BundledExchangesParser> parser_remote;
  BundledExchangesParser parser_impl(
      parser_remote.InitWithNewPipeAndPassReceiver(), std::move(source_remote));
  data_decoder::mojom::BundledExchangesParser& parser = parser_impl;

  base::RunLoop run_loop;
  mojom::BundleResponsePtr result;
  parser.ParseResponse(
      item->response_offset, item->response_length,
      base::BindLambdaForTesting(
          [&result, &run_loop](mojom::BundleResponsePtr response,
                               const base::Optional<std::string>& error) {
            result = std::move(response);
            run_loop.QuitClosure().Run();
          }));
  run_loop.Run();
  return result;
}

class BundleBuilder {
 public:
  using Headers = std::vector<std::pair<std::string, std::string>>;

  void AddExchange(const Headers& request_headers,
                   const Headers& response_headers,
                   base::StringPiece payload) {
    cbor::Value::ArrayValue response_array;
    response_array.emplace_back(
        *cbor::Writer::Write(CreateHeaderMap(response_headers)));
    response_array.emplace_back(CreateByteString(payload));
    cbor::Value response(response_array);

    index_.emplace_back(CreateHeaderMap(request_headers));
    index_.emplace_back(EncodedLength(response));
    responses_.emplace_back(std::move(response));
  }

  void AddSection(base::StringPiece name, cbor::Value section) {
    section_lengths_.emplace_back(name);
    section_lengths_.emplace_back(EncodedLength(section));
    sections_.emplace_back(std::move(section));
  }

  std::vector<uint8_t> CreateBundle() {
    AddSection("index", cbor::Value(index_));
    AddSection("responses", cbor::Value(responses_));
    return *cbor::Writer::Write(CreateTopLevel());
  }

 private:
  static cbor::Value CreateByteString(base::StringPiece s) {
    return cbor::Value(base::as_bytes(base::make_span(s)));
  }

  static cbor::Value CreateHeaderMap(const Headers& headers) {
    cbor::Value::MapValue map;
    for (const auto& pair : headers)
      map.insert({CreateByteString(pair.first), CreateByteString(pair.second)});
    return cbor::Value(std::move(map));
  }

  cbor::Value CreateTopLevel() {
    cbor::Value::ArrayValue toplevel_array;
    toplevel_array.emplace_back(
        CreateByteString(u8"\U0001F310\U0001F4E6"));  // "🌐📦"
    toplevel_array.emplace_back(
        *cbor::Writer::Write(cbor::Value(section_lengths_)));
    toplevel_array.emplace_back(sections_);
    toplevel_array.emplace_back(CreateByteString(""));  // length (ignored)
    return cbor::Value(toplevel_array);
  }

  static int64_t EncodedLength(const cbor::Value& value) {
    return cbor::Writer::Write(value)->size();
  }

  cbor::Value::ArrayValue section_lengths_;
  cbor::Value::ArrayValue sections_;
  cbor::Value::ArrayValue index_;
  cbor::Value::ArrayValue responses_;
};

}  // namespace

class BundledExchangeParserTest : public testing::Test {
 private:
  base::test::ScopedTaskEnvironment task_environment_;
};

TEST_F(BundledExchangeParserTest, EmptyBundle) {
  BundleBuilder builder;
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source);
  ASSERT_TRUE(metadata);
  const auto& index = metadata->index;

  ASSERT_EQ(index.size(), 0u);
}

TEST_F(BundledExchangeParserTest, WrongMagic) {
  BundleBuilder builder;
  std::vector<uint8_t> bundle = builder.CreateBundle();
  bundle[3] ^= 1;
  TestDataSource data_source(bundle);

  ASSERT_FALSE(ParseBundle(&data_source));
}

TEST_F(BundledExchangeParserTest, SectionLengthsTooLarge) {
  BundleBuilder builder;
  std::string too_long_section_name(8192, 'x');
  builder.AddSection(too_long_section_name, cbor::Value(0));
  TestDataSource data_source(builder.CreateBundle());

  ASSERT_FALSE(ParseBundle(&data_source));
}

TEST_F(BundledExchangeParserTest, DuplicateSectionName) {
  BundleBuilder builder;
  builder.AddSection("foo", cbor::Value(0));
  builder.AddSection("foo", cbor::Value(0));
  TestDataSource data_source(builder.CreateBundle());

  ASSERT_FALSE(ParseBundle(&data_source));
}

TEST_F(BundledExchangeParserTest, SingleEntry) {
  BundleBuilder builder;
  builder.AddExchange(
      {{":method", "GET"}, {":url", "https://test.example.com/"}},
      {{":status", "200"}, {"content-type", "text/plain"}}, "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source);
  ASSERT_TRUE(metadata);
  const auto& index = metadata->index;

  ASSERT_EQ(index.size(), 1u);
  auto response = ParseResponse(&data_source, index[0]);
  ASSERT_TRUE(response);

  EXPECT_EQ(index[0]->request_url, "https://test.example.com/");
  EXPECT_EQ(index[0]->request_method, "GET");
  EXPECT_EQ(index[0]->request_headers.size(), 0u);
  EXPECT_EQ(response->response_code, 200);
  EXPECT_EQ(response->response_headers.size(), 1u);
  EXPECT_EQ(response->response_headers["content-type"], "text/plain");
  EXPECT_EQ(data_source.GetPayload(response), "payload");
}

TEST_F(BundledExchangeParserTest, InvalidRequestURL) {
  BundleBuilder builder;
  builder.AddExchange({{":method", "GET"}, {":url", ""}},
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source);
  ASSERT_FALSE(metadata);
}

TEST_F(BundledExchangeParserTest, RequestURLHasCredentials) {
  BundleBuilder builder;
  builder.AddExchange(
      {{":method", "GET"}, {":url", "https://user:passwd@test.example.com/"}},
      {{":status", "200"}, {"content-type", "text/plain"}}, "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source);
  ASSERT_FALSE(metadata);
}

TEST_F(BundledExchangeParserTest, RequestURLHasFragment) {
  BundleBuilder builder;
  builder.AddExchange(
      {{":method", "GET"}, {":url", "https://test.example.com/#fragment"}},
      {{":status", "200"}, {"content-type", "text/plain"}}, "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source);
  ASSERT_FALSE(metadata);
}

TEST_F(BundledExchangeParserTest, NoMethodInRequestHeaders) {
  BundleBuilder builder;
  builder.AddExchange(
      {{":url", "https://test.example.com/"}},  // ":method" is missing.
      {{":status", "200"}, {"content-type", "text/plain"}}, "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source);
  ASSERT_FALSE(metadata);
}

TEST_F(BundledExchangeParserTest, MethodIsNotGET) {
  BundleBuilder builder;
  builder.AddExchange(
      {{":method", "POST"}, {":url", "https://test.example.com/"}},
      {{":status", "200"}, {"content-type", "text/plain"}}, "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source);
  ASSERT_FALSE(metadata);
}

TEST_F(BundledExchangeParserTest, ExtraPseudoInRequestHeaders) {
  BundleBuilder builder;
  builder.AddExchange({{":method", "GET"},
                       {":url", "https://test.example.com/"},
                       {":scheme", "https"}},
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ASSERT_FALSE(ParseBundle(&data_source));
}

TEST_F(BundledExchangeParserTest, NoStatusInResponseHeaders) {
  BundleBuilder builder;
  builder.AddExchange(
      {{":method", "GET"}, {":url", "https://test.example.com/"}},
      {{"content-type", "text/plain"}}, "payload");  // ":status" is missing.
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source);
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->index.size(), 1u);

  ASSERT_FALSE(ParseResponse(&data_source, metadata->index[0]));
}

TEST_F(BundledExchangeParserTest, InvalidResponseStatus) {
  BundleBuilder builder;
  builder.AddExchange(
      {{":method", "GET"}, {":url", "https://test.example.com/"}},
      {{":status", "0200"}, {"content-type", "text/plain"}}, "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source);
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->index.size(), 1u);

  ASSERT_FALSE(ParseResponse(&data_source, metadata->index[0]));
}

TEST_F(BundledExchangeParserTest, ExtraPseudoInResponseHeaders) {
  BundleBuilder builder;
  builder.AddExchange(
      {{":method", "GET"}, {":url", "https://test.example.com/"}},
      {{":status", "200"}, {":foo", ""}, {"content-type", "text/plain"}},
      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source);
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->index.size(), 1u);

  ASSERT_FALSE(ParseResponse(&data_source, metadata->index[0]));
}

TEST_F(BundledExchangeParserTest, UpperCaseCharacterInHeaderName) {
  BundleBuilder builder;
  builder.AddExchange(
      {{":method", "GET"}, {":url", "https://test.example.com/"}},
      {{":status", "200"}, {"Content-Type", "text/plain"}}, "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source);
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->index.size(), 1u);

  ASSERT_FALSE(ParseResponse(&data_source, metadata->index[0]));
}

TEST_F(BundledExchangeParserTest, InvalidHeaderValue) {
  BundleBuilder builder;
  builder.AddExchange(
      {{":method", "GET"}, {":url", "https://test.example.com/"}},
      {{":status", "200"}, {"content-type", "\n"}}, "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source);
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->index.size(), 1u);

  ASSERT_FALSE(ParseResponse(&data_source, metadata->index[0]));
}

TEST_F(BundledExchangeParserTest, ParseGoldenFile) {
  TestDataSource data_source(base::FilePath(FILE_PATH_LITERAL("hello.wbn")));

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source);
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->index.size(), 4u);

  const auto& index = metadata->index;

  std::vector<mojom::BundleResponsePtr> responses;
  for (size_t i = 0; i < index.size(); i++) {
    responses.push_back(ParseResponse(&data_source, index[i]));
    ASSERT_TRUE(responses.back());
  }

  EXPECT_EQ(index[0]->request_url, "https://test.example.org/");
  EXPECT_EQ(index[0]->request_method, "GET");
  EXPECT_EQ(index[0]->request_headers.size(), 0u);
  EXPECT_EQ(responses[0]->response_code, 200);
  EXPECT_EQ(responses[0]->response_headers["content-type"],
            "text/html; charset=utf-8");
  EXPECT_EQ(data_source.GetPayload(responses[0]),
            GetTestFileContents(
                base::FilePath(FILE_PATH_LITERAL("hello/index.html"))));

  EXPECT_EQ(index[1]->request_url, "https://test.example.org/index.html");
  EXPECT_EQ(index[2]->request_url,
            "https://test.example.org/manifest.webmanifest");
  EXPECT_EQ(index[3]->request_url, "https://test.example.org/script.js");
}

}  // namespace data_decoder
