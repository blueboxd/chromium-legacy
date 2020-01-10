// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/file_url_loader_factory.h"

#include <memory>
#include <string>

#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "net/base/filename_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

GURL GetTestURL(const std::string& filename) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath path;
  base::PathService::Get(DIR_TEST_DATA, &path);
  path = path.AppendASCII("loader");
  return net::FilePathToFileURL(path.AppendASCII(filename));
}

class FileURLLoaderFactoryTest : public testing::Test {
 public:
  FileURLLoaderFactoryTest()
      : access_list_(SharedCorsOriginAccessList::Create()),
        factory_(std::make_unique<FileURLLoaderFactory>(
            profile_dummy_path_,
            /*shared_cors_origin_access_list=*/nullptr,
            base::TaskPriority::BEST_EFFORT)) {}
  FileURLLoaderFactoryTest(const FileURLLoaderFactoryTest&) = delete;
  FileURLLoaderFactoryTest& operator=(const FileURLLoaderFactoryTest&) = delete;
  ~FileURLLoaderFactoryTest() override = default;

 protected:
  int CreateLoaderAndRunWithRequestMode(
      network::mojom::RequestMode request_mode) {
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = GetTestURL("get.txt");
    request->mode = request_mode;

    auto loader = network::SimpleURLLoader::Create(
        std::move(request), TRAFFIC_ANNOTATION_FOR_TESTS);

    SimpleURLLoaderTestHelper helper;
    loader->DownloadToString(
        factory_.get(), helper.GetCallback(),
        network::SimpleURLLoader::kMaxBoundedStringDownloadSize);

    helper.WaitForCallback();
    return loader->NetError();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::FilePath profile_dummy_path_;
  scoped_refptr<SharedCorsOriginAccessList> access_list_;
  std::unique_ptr<network::mojom::URLLoaderFactory> factory_;
};

TEST_F(FileURLLoaderFactoryTest, MissedRequestInitiator) {
  // CORS-disabled requests can omit |request.request_initiator| though it is
  // discouraged not to set |request.request_initiator|.
  EXPECT_EQ(net::OK, CreateLoaderAndRunWithRequestMode(
                         network::mojom::RequestMode::kSameOrigin));

  EXPECT_EQ(net::OK, CreateLoaderAndRunWithRequestMode(
                         network::mojom::RequestMode::kNoCors));

  EXPECT_EQ(net::OK, CreateLoaderAndRunWithRequestMode(
                         network::mojom::RequestMode::kNavigate));

  // CORS-enabled requests need |request.request_initiator| set.
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, CreateLoaderAndRunWithRequestMode(
                                           network::mojom::RequestMode::kCors));

  EXPECT_EQ(net::ERR_INVALID_ARGUMENT,
            CreateLoaderAndRunWithRequestMode(
                network::mojom::RequestMode::kCorsWithForcedPreflight));
}

}  // namespace

}  // namespace content
