// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_image.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"

namespace {
constexpr int kTestBitmapWidth = 200;
constexpr int kTestBitmapHeight = 123;

// Waits for thumbnail images and can report how many images it has received.
class TestThumbnailImageObserver : public ThumbnailImage::Observer {
 public:
  void WaitForImage() {
    if (new_image_count_ > last_image_count_) {
      last_image_count_ = new_image_count_;
      return;
    }

    // Need a fresh loop since we may have quit out of the last one.
    run_loop_ = std::make_unique<base::RunLoop>();
    waiting_ = true;
    run_loop_->Run();
  }

  int new_image_count() const { return new_image_count_; }
  gfx::ImageSkia thumbnail_image() const { return thumbnail_image_; }

  ScopedObserver<ThumbnailImage, ThumbnailImage::Observer>* scoped_observer() {
    return &scoped_observer_;
  }

 private:
  // ThumbnailImage::Observer:
  void OnThumbnailImageAvailable(gfx::ImageSkia thumbnail_image) override {
    ++new_image_count_;
    thumbnail_image_ = thumbnail_image;
    if (waiting_) {
      last_image_count_ = new_image_count_;
      run_loop_->Quit();
      waiting_ = false;
    }
  }

  ScopedObserver<ThumbnailImage, ThumbnailImage::Observer> scoped_observer_{
      this};
  int new_image_count_ = 0;
  int last_image_count_ = 0;
  gfx::ImageSkia thumbnail_image_;
  bool waiting_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace

class ThumbnailImageTest : public testing::Test,
                           public ThumbnailImage::Delegate {
 public:
  ThumbnailImageTest() = default;

 protected:
  static SkBitmap CreateBitmap(int width, int height) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    bitmap.eraseARGB(255, 0, 255, 0);
    return bitmap;
  }

  bool is_being_observed() const { return is_being_observed_; }

 private:
  void ThumbnailImageBeingObservedChanged(bool is_being_observed) override {
    is_being_observed_ = is_being_observed;
  }

  bool is_being_observed_ = false;
  base::test::TaskEnvironment task_environment_;
  DISALLOW_COPY_AND_ASSIGN(ThumbnailImageTest);
};

TEST_F(ThumbnailImageTest, Add_Remove_Observer) {
  auto image = base::MakeRefCounted<ThumbnailImage>(this);
  EXPECT_FALSE(is_being_observed());
  TestThumbnailImageObserver observer;
  image->AddObserver(&observer);
  EXPECT_TRUE(image->HasObserver(&observer));
  EXPECT_TRUE(is_being_observed());
  image->RemoveObserver(&observer);
  EXPECT_FALSE(image->HasObserver(&observer));
  EXPECT_FALSE(is_being_observed());
}

TEST_F(ThumbnailImageTest, Add_Remove_MultipleObservers) {
  auto image = base::MakeRefCounted<ThumbnailImage>(this);
  EXPECT_FALSE(is_being_observed());
  TestThumbnailImageObserver observer;
  TestThumbnailImageObserver observer2;
  image->AddObserver(&observer);
  EXPECT_TRUE(image->HasObserver(&observer));
  EXPECT_TRUE(is_being_observed());
  image->AddObserver(&observer2);
  EXPECT_TRUE(image->HasObserver(&observer2));
  EXPECT_TRUE(is_being_observed());
  image->RemoveObserver(&observer);
  EXPECT_FALSE(image->HasObserver(&observer));
  EXPECT_TRUE(image->HasObserver(&observer2));
  EXPECT_TRUE(is_being_observed());
  image->RemoveObserver(&observer2);
  EXPECT_FALSE(image->HasObserver(&observer2));
  EXPECT_FALSE(is_being_observed());
}

TEST_F(ThumbnailImageTest, AssignSkBitmap_NotifiesObservers) {
  auto image = base::MakeRefCounted<ThumbnailImage>(this);
  TestThumbnailImageObserver observer;
  TestThumbnailImageObserver observer2;
  observer.scoped_observer()->Add(image.get());
  observer2.scoped_observer()->Add(image.get());

  SkBitmap bitmap = CreateBitmap(kTestBitmapWidth, kTestBitmapHeight);
  image->AssignSkBitmap(bitmap);
  observer.WaitForImage();
  observer2.WaitForImage();
  EXPECT_EQ(1, observer.new_image_count());
  EXPECT_EQ(1, observer2.new_image_count());
  EXPECT_FALSE(observer.thumbnail_image().isNull());
  EXPECT_FALSE(observer2.thumbnail_image().isNull());
  EXPECT_EQ(gfx::Size(kTestBitmapWidth, kTestBitmapHeight),
            observer.thumbnail_image().size());
}

TEST_F(ThumbnailImageTest, AssignSkBitmap_NotifiesObserversAgain) {
  auto image = base::MakeRefCounted<ThumbnailImage>(this);
  TestThumbnailImageObserver observer;
  TestThumbnailImageObserver observer2;
  observer.scoped_observer()->Add(image.get());
  observer2.scoped_observer()->Add(image.get());

  SkBitmap bitmap = CreateBitmap(kTestBitmapWidth, kTestBitmapHeight);
  image->AssignSkBitmap(bitmap);
  observer.WaitForImage();
  observer2.WaitForImage();
  image->AssignSkBitmap(bitmap);
  observer.WaitForImage();
  observer2.WaitForImage();
  EXPECT_EQ(2, observer.new_image_count());
  EXPECT_EQ(2, observer2.new_image_count());
  EXPECT_FALSE(observer.thumbnail_image().isNull());
  EXPECT_FALSE(observer2.thumbnail_image().isNull());
  EXPECT_EQ(gfx::Size(kTestBitmapWidth, kTestBitmapHeight),
            observer.thumbnail_image().size());
}

TEST_F(ThumbnailImageTest, RequestThumbnailImage) {
  auto image = base::MakeRefCounted<ThumbnailImage>(this);
  TestThumbnailImageObserver observer;
  observer.scoped_observer()->Add(image.get());

  SkBitmap bitmap = CreateBitmap(kTestBitmapWidth, kTestBitmapHeight);
  image->AssignSkBitmap(bitmap);
  observer.WaitForImage();

  TestThumbnailImageObserver observer2;
  observer2.scoped_observer()->Add(image.get());
  image->RequestThumbnailImage();
  observer.WaitForImage();
  observer2.WaitForImage();
  EXPECT_EQ(2, observer.new_image_count());
  EXPECT_EQ(1, observer2.new_image_count());
  EXPECT_FALSE(observer2.thumbnail_image().isNull());
  EXPECT_EQ(gfx::Size(kTestBitmapWidth, kTestBitmapHeight),
            observer2.thumbnail_image().size());
}
