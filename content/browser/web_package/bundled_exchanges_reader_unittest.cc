// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/bundled_exchanges_reader.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/test/scoped_task_environment.h"
#include "content/browser/web_package/bundled_exchanges_source.h"
#include "mojo/public/c/system/data_pipe.h"
#include "services/data_decoder/public/mojom/bundled_exchanges_parser.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class MockParser final : public data_decoder::mojom::BundledExchangesParser {
 public:
  MockParser(mojo::PendingReceiver<data_decoder::mojom::BundledExchangesParser>
                 receiver)
      : receiver_(this, std::move(receiver)) {}
  ~MockParser() override {}

  void RunMetadataCallback(data_decoder::mojom::BundleMetadataPtr metadata) {
    base::RunLoop().RunUntilIdle();
    std::move(metadata_callback_).Run(std::move(metadata), base::nullopt);
  }
  void RunResponseCallback(data_decoder::mojom::BundleResponsePtr response) {
    base::RunLoop().RunUntilIdle();
    std::move(response_callback_).Run(std::move(response), base::nullopt);
  }

  void WaitUntilParseMetadataCalled(base::OnceClosure closure) {
    if (metadata_callback_.is_null())
      wait_parse_metadata_callback_ = std::move(closure);
    else
      std::move(closure).Run();
  }

 private:
  // data_decoder::mojom::BundledExchangesParser implementation.
  void ParseMetadata(ParseMetadataCallback callback) override {
    metadata_callback_ = std::move(callback);
    if (!wait_parse_metadata_callback_.is_null())
      std::move(wait_parse_metadata_callback_).Run();
  }
  void ParseResponse(uint64_t response_offset,
                     uint64_t response_length,
                     ParseResponseCallback callback) override {
    response_callback_ = std::move(callback);
  }

  mojo::Receiver<data_decoder::mojom::BundledExchangesParser> receiver_;

  ParseMetadataCallback metadata_callback_;
  ParseResponseCallback response_callback_;
  base::OnceClosure wait_parse_metadata_callback_;
};

class MockParserFactory final
    : public data_decoder::mojom::BundledExchangesParserFactory {
 public:
  MockParserFactory() {}
  ~MockParserFactory() override {}

  void WaitUntilParseMetadataCalled(base::OnceClosure closure) {
    if (parser_)
      parser_->WaitUntilParseMetadataCalled(std::move(closure));
    else
      wait_parse_metadata_callback_ = std::move(closure);
  }

  void RunMetadataCallback(data_decoder::mojom::BundleMetadataPtr metadata) {
    base::RunLoop run_loop;
    WaitUntilParseMetadataCalled(run_loop.QuitClosure());
    run_loop.Run();

    ASSERT_TRUE(parser_);
    parser_->RunMetadataCallback(std::move(metadata));
  }
  void RunResponseCallback(data_decoder::mojom::BundleResponsePtr response) {
    ASSERT_TRUE(parser_);
    parser_->RunResponseCallback(std::move(response));
  }

 private:
  // data_decoder::mojom::BundledExchangesParserFactory implementation.
  void GetParserForFile(
      mojo::PendingReceiver<data_decoder::mojom::BundledExchangesParser>
          receiver,
      base::File file) override {
    parser_ = std::make_unique<MockParser>(std::move(receiver));
    if (!wait_parse_metadata_callback_.is_null()) {
      parser_->WaitUntilParseMetadataCalled(
          std::move(wait_parse_metadata_callback_));
    }
  }
  void GetParserForDataSource(
      mojo::PendingReceiver<data_decoder::mojom::BundledExchangesParser>
          receiver,
      mojo::PendingRemote<data_decoder::mojom::BundleDataSource> data_source)
      override {
    NOTREACHED();
  }

  std::unique_ptr<MockParser> parser_;
  base::OnceClosure wait_parse_metadata_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockParserFactory);
};

class BundledExchangesReaderTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(
        CreateTemporaryFileInDir(temp_dir_.GetPath(), &temp_file_path_));

    ASSERT_EQ(base::checked_cast<int>(body_.size()),
              base::WriteFile(temp_file_path_, body_.data(), body_.size()));

    BundledExchangesSource source(temp_file_path_);
    reader_ = std::make_unique<BundledExchangesReader>(source);

    std::unique_ptr<MockParserFactory> factory =
        std::make_unique<MockParserFactory>();
    factory_ = factory.get();
    reader_->SetBundledExchangesParserFactoryForTesting(std::move(factory));
  }

 protected:
  void ReadMetadata() {
    base::RunLoop run_loop;
    reader_->ReadMetadata(base::BindOnce(
        [](base::Closure quit_closure, base::Optional<std::string> error) {
          EXPECT_FALSE(error);
          std::move(quit_closure).Run();
        },
        run_loop.QuitClosure()));

    std::vector<data_decoder::mojom::BundleIndexItemPtr> items;
    data_decoder::mojom::BundleIndexItemPtr item =
        data_decoder::mojom::BundleIndexItem::New();
    item->request_url = start_url_;
    item->response_offset = 573u;
    item->response_length = 765u;
    items.push_back(std::move(item));

    data_decoder::mojom::BundleMetadataPtr metadata =
        data_decoder::mojom::BundleMetadata::New(std::move(items), GURL());
    factory_->RunMetadataCallback(std::move(metadata));
    run_loop.Run();
  }
  BundledExchangesReader* GetReader() { return reader_.get(); }
  MockParserFactory* GetFactory() { return factory_; }
  const GURL& GetStartURL() const { return start_url_; }
  const std::string& GetBody() const { return body_; }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath temp_file_path_;
  std::unique_ptr<BundledExchangesReader> reader_;
  MockParserFactory* factory_;
  const GURL start_url_ = GURL("https://www.google.com/");
  const std::string body_ = std::string("hello new open world.");
};

TEST_F(BundledExchangesReaderTest, ReadMetadata) {
  ReadMetadata();
  EXPECT_EQ(GetStartURL(), GetReader()->GetStartURL());
  EXPECT_TRUE(GetReader()->HasEntry(GetStartURL()));
  EXPECT_FALSE(GetReader()->HasEntry(GURL("https://www.google.com/404")));
}

TEST_F(BundledExchangesReaderTest, ReadResponse) {
  ReadMetadata();
  ASSERT_TRUE(GetReader()->HasEntry(GetStartURL()));

  base::RunLoop run_loop;
  GetReader()->ReadResponse(
      GetStartURL(), base::BindOnce(
                         [](base::Closure quit_closure,
                            data_decoder::mojom::BundleResponsePtr response) {
                           EXPECT_TRUE(response);
                           if (response) {
                             EXPECT_EQ(200, response->response_code);
                             EXPECT_EQ(0xdeadu, response->payload_offset);
                             EXPECT_EQ(0xbeafu, response->payload_length);
                           }
                           std::move(quit_closure).Run();
                         },
                         run_loop.QuitClosure()));

  data_decoder::mojom::BundleResponsePtr response =
      data_decoder::mojom::BundleResponse::New();
  response->response_code = 200;
  response->payload_offset = 0xdead;
  response->payload_length = 0xbeaf;
  GetFactory()->RunResponseCallback(std::move(response));
  run_loop.Run();
}

TEST_F(BundledExchangesReaderTest, ReadResponseBody) {
  ReadMetadata();

  data_decoder::mojom::BundleResponsePtr response =
      data_decoder::mojom::BundleResponse::New();
  constexpr size_t expected_offset = 4;
  const size_t expected_length = GetBody().size() - 8;
  response->payload_offset = expected_offset;
  response->payload_length = expected_length;

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.element_num_bytes = 1;
  options.capacity_num_bytes = response->payload_length + 1;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(&options, &producer, &consumer));

  base::RunLoop run_loop;
  MojoResult callback_result;
  GetReader()->ReadResponseBody(
      std::move(response), std::move(producer),
      base::BindOnce(
          [](base::Closure quit_closure, MojoResult* callback_result,
             MojoResult result) {
            *callback_result = result;
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure(), &callback_result));
  run_loop.Run();
  ASSERT_EQ(MOJO_RESULT_OK, callback_result);

  std::vector<char> buffer(expected_length);
  uint32_t bytes_read = buffer.size();
  MojoResult read_result =
      consumer->ReadData(buffer.data(), &bytes_read, /*flags=*/0);
  ASSERT_EQ(MOJO_RESULT_OK, read_result);
  ASSERT_EQ(buffer.size(), bytes_read);
  EXPECT_EQ(GetBody().substr(expected_offset, expected_length),
            std::string(buffer.data(), bytes_read));
}

}  // namespace

}  // namespace content
