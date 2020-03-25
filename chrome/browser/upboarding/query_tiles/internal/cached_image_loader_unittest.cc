// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/internal/cached_image_loader.h"

#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

using ::testing::_;
using ::testing::Invoke;

namespace upboarding {
namespace {

class CachedImageLoaderTest : public testing::Test {
 public:
  CachedImageLoaderTest() = default;
  ~CachedImageLoaderTest() override = default;

  void SetUp() override {
    image_loader_ = std::make_unique<CachedImageLoader>(&mock_fetcher_);
  }

 protected:
  void FetchImage() {
    image_loader_->FetchImage(
        GURL("https://www.example.com/dummy_image"),
        base::BindOnce(&CachedImageLoaderTest::OnBitmapFetched,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  image_fetcher::MockImageFetcher* mock_fetcher() { return &mock_fetcher_; }

  const SkBitmap& result() const { return result_; }

 private:
  void OnBitmapFetched(SkBitmap bitmap) { result_ = bitmap; }

  base::test::TaskEnvironment task_environment_;
  image_fetcher::MockImageFetcher mock_fetcher_;

  std::unique_ptr<ImageLoader> image_loader_;
  SkBitmap result_;

  base::WeakPtrFactory<CachedImageLoaderTest> weak_ptr_factory_{this};
};

TEST_F(CachedImageLoaderTest, FetchImage) {
  // Create a non-empty bitmap.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(32, 16);
  EXPECT_FALSE(bitmap.empty());
  EXPECT_EQ(bitmap.width(), 32);
  auto image = gfx::Image::CreateFrom1xBitmap(bitmap);

  EXPECT_CALL(*mock_fetcher(), FetchImageAndData_(_, _, _, _))
      .WillRepeatedly(
          Invoke([&image](const GURL&, image_fetcher::ImageDataFetcherCallback*,
                          image_fetcher::ImageFetcherCallback* fetch_callback,
                          image_fetcher::ImageFetcherParams) {
            std::move(*fetch_callback)
                .Run(image, image_fetcher::RequestMetadata());
          }));
  FetchImage();
  EXPECT_FALSE(result().empty());
  EXPECT_EQ(result().width(), 32);
}

}  // namespace
}  // namespace upboarding
