// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/mock_web_bundle_reader_factory.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "content/browser/web_package/web_bundle_reader.h"
#include "content/browser/web_package/web_bundle_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/filename_util.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/data_decoder/public/mojom/web_bundle_parser.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class MockParser final : public data_decoder::mojom::WebBundleParser {
 public:
  MockParser(
      mojo::PendingReceiver<data_decoder::mojom::WebBundleParser> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~MockParser() override = default;

  void RunMetadataCallback(data_decoder::mojom::BundleMetadataPtr metadata) {
    std::move(metadata_callback_).Run(std::move(metadata), nullptr);
  }
  void RunResponseCallback(data_decoder::mojom::BundleResponsePtr response) {
    std::move(response_callback_).Run(std::move(response), nullptr);
  }

  void WaitUntilParseMetadataCalled(base::OnceClosure closure) {
    if (metadata_callback_.is_null())
      wait_parse_metadata_callback_ = std::move(closure);
    else
      std::move(closure).Run();
  }

  void WaitUntilParseResponseCalled(
      base::OnceCallback<void(data_decoder::mojom::BundleResponseLocationPtr)>
          callback) {
    if (response_callback_.is_null())
      wait_parse_response_callback_ = std::move(callback);
    else
      std::move(callback).Run(std::move(parse_response_args_));
  }

 private:
  // data_decoder::mojom::WebBundleParser implementation.
  void ParseMetadata(ParseMetadataCallback callback) override {
    metadata_callback_ = std::move(callback);
    if (!wait_parse_metadata_callback_.is_null())
      std::move(wait_parse_metadata_callback_).Run();
  }
  void ParseResponse(uint64_t response_offset,
                     uint64_t response_length,
                     ParseResponseCallback callback) override {
    response_callback_ = std::move(callback);
    parse_response_args_ = data_decoder::mojom::BundleResponseLocation::New(
        response_offset, response_length);
    if (!wait_parse_response_callback_.is_null()) {
      std::move(wait_parse_response_callback_)
          .Run(std::move(parse_response_args_));
    }
  }

  mojo::Receiver<data_decoder::mojom::WebBundleParser> receiver_;

  ParseMetadataCallback metadata_callback_;
  ParseResponseCallback response_callback_;
  data_decoder::mojom::BundleResponseLocationPtr parse_response_args_;
  base::OnceClosure wait_parse_metadata_callback_;
  base::OnceCallback<void(data_decoder::mojom::BundleResponseLocationPtr)>
      wait_parse_response_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockParser);
};

class MockParserFactory final
    : public data_decoder::mojom::WebBundleParserFactory {
 public:
  MockParserFactory() {}
  ~MockParserFactory() override = default;

  void AddReceiver(
      mojo::PendingReceiver<data_decoder::mojom::WebBundleParserFactory>
          receiver) {
    receivers_.Add(this, std::move(receiver));
  }

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

  void RunResponseCallback(
      data_decoder::mojom::BundleResponseLocationPtr expected_parse_args,
      data_decoder::mojom::BundleResponsePtr response) {
    ASSERT_TRUE(parser_);
    base::RunLoop run_loop;
    parser_->WaitUntilParseResponseCalled(base::BindLambdaForTesting(
        [&run_loop, &expected_parse_args](
            data_decoder::mojom::BundleResponseLocationPtr parse_args) {
          EXPECT_EQ(expected_parse_args->offset, parse_args->offset);
          EXPECT_EQ(expected_parse_args->length, parse_args->length);
          run_loop.Quit();
        }));
    run_loop.Run();
    parser_->RunResponseCallback(std::move(response));
  }

 private:
  // data_decoder::mojom::WebBundleParserFactory implementation.
  void GetParserForFile(
      mojo::PendingReceiver<data_decoder::mojom::WebBundleParser> receiver,
      base::File file) override {
    parser_ = std::make_unique<MockParser>(std::move(receiver));
    if (!wait_parse_metadata_callback_.is_null()) {
      parser_->WaitUntilParseMetadataCalled(
          std::move(wait_parse_metadata_callback_));
    }
  }
  void GetParserForDataSource(
      mojo::PendingReceiver<data_decoder::mojom::WebBundleParser> receiver,
      mojo::PendingRemote<data_decoder::mojom::BundleDataSource> data_source)
      override {
    NOTREACHED();
  }

  std::unique_ptr<MockParser> parser_;
  mojo::ReceiverSet<data_decoder::mojom::WebBundleParserFactory> receivers_;
  base::OnceClosure wait_parse_metadata_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockParserFactory);
};

class MockWebBundleReaderFactoryImpl final : public MockWebBundleReaderFactory {
 public:
  MockWebBundleReaderFactoryImpl() : MockWebBundleReaderFactory() {}
  ~MockWebBundleReaderFactoryImpl() override = default;

  scoped_refptr<WebBundleReader> CreateReader(
      const std::string& test_file_data) override {
    DCHECK(!factory_);
    if (!temp_dir_.CreateUniqueTempDir() ||
        !CreateTemporaryFileInDir(temp_dir_.GetPath(), &temp_file_path_) ||
        (test_file_data.size() !=
         static_cast<size_t>(base::WriteFile(
             temp_file_path_, test_file_data.data(), test_file_data.size())))) {
      return nullptr;
    }

    auto reader = base::MakeRefCounted<WebBundleReader>(
        WebBundleSource::MaybeCreateFromTrustedFileUrl(
            net::FilePathToFileURL(temp_file_path_)));

    factory_ = std::make_unique<MockParserFactory>();
    in_process_data_decoder_.service()
        .SetWebBundleParserFactoryBinderForTesting(base::BindRepeating(
            &MockParserFactory::AddReceiver, base::Unretained(factory_.get())));
    return reader;
  }

  void ReadAndFullfillMetadata(
      WebBundleReader* reader,
      data_decoder::mojom::BundleMetadataPtr metadata,
      WebBundleReader::MetadataCallback callback) override {
    ASSERT_TRUE(factory_);
    DCHECK(reader);

    base::RunLoop run_loop;
    reader->ReadMetadata(base::BindOnce(
        [](base::OnceClosure quit_closure,
           WebBundleReader::MetadataCallback callback,
           data_decoder::mojom::BundleMetadataParseErrorPtr error) {
          std::move(callback).Run(std::move(error));
          std::move(quit_closure).Run();
        },
        run_loop.QuitClosure(), std::move(callback)));

    factory_->RunMetadataCallback(std::move(metadata));
    run_loop.Run();
  }

  void ReadAndFullfillResponse(
      WebBundleReader* reader,
      const network::ResourceRequest& resource_request,
      data_decoder::mojom::BundleResponseLocationPtr expected_parse_args,
      data_decoder::mojom::BundleResponsePtr response,
      WebBundleReader::ResponseCallback callback) override {
    ASSERT_TRUE(factory_);
    DCHECK(reader);

    base::RunLoop run_loop;
    reader->ReadResponse(
        resource_request,
        base::BindOnce(
            [](base::OnceClosure quit_closure,
               WebBundleReader::ResponseCallback callback,
               data_decoder::mojom::BundleResponsePtr response,
               data_decoder::mojom::BundleResponseParseErrorPtr error) {
              std::move(callback).Run(std::move(response), std::move(error));
              std::move(quit_closure).Run();
            },
            run_loop.QuitClosure(), std::move(callback)));

    factory_->RunResponseCallback(std::move(expected_parse_args),
                                  std::move(response));
    run_loop.Run();
  }

  void FullfillResponse(
      data_decoder::mojom::BundleResponseLocationPtr expected_parse_args,
      data_decoder::mojom::BundleResponsePtr response) override {
    ASSERT_TRUE(factory_);

    factory_->RunResponseCallback(std::move(expected_parse_args),
                                  std::move(response));
  }

 private:
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::string test_file_data_;
  base::ScopedTempDir temp_dir_;
  base::FilePath temp_file_path_;
  std::unique_ptr<MockParserFactory> factory_;

  DISALLOW_COPY_AND_ASSIGN(MockWebBundleReaderFactoryImpl);
};

}  // namespace

// static
std::unique_ptr<MockWebBundleReaderFactory>
MockWebBundleReaderFactory::Create() {
  return std::make_unique<MockWebBundleReaderFactoryImpl>();
}

}  // namespace content
