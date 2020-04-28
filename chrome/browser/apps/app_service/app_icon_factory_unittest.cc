// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class AppIconFactoryTest : public testing::Test {
 public:
  base::FilePath GetPath() {
    return tmp_dir_.GetPath().Append(
        base::FilePath::FromUTF8Unsafe("icon.file"));
  }

  void SetUp() override { ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir()); }

  void RunLoadIconFromFileWithFallback(
      apps::mojom::IconValuePtr fallback_response,
      bool* callback_called,
      bool* fallback_called,
      apps::mojom::IconValuePtr* result) {
    *callback_called = false;
    *fallback_called = false;

    apps::LoadIconFromFileWithFallback(
        apps::mojom::IconCompression::kUncompressed, 200, GetPath(),
        apps::IconEffects::kNone,
        base::BindOnce(
            [](bool* called, apps::mojom::IconValuePtr* result,
               base::OnceClosure quit, apps::mojom::IconValuePtr icon) {
              *called = true;
              *result = std::move(icon);
              std::move(quit).Run();
            },
            base::Unretained(callback_called), base::Unretained(result),
            run_loop_.QuitClosure()),
        base::BindOnce(
            [](bool* called, apps::mojom::IconValuePtr response,
               apps::mojom::Publisher::LoadIconCallback callback) {
              *called = true;
              std::move(callback).Run(std::move(response));
            },
            base::Unretained(fallback_called), std::move(fallback_response)));

    run_loop_.Run();
  }

 protected:
  content::BrowserTaskEnvironment task_env_{};
  base::ScopedTempDir tmp_dir_{};
  base::RunLoop run_loop_{};
};

TEST_F(AppIconFactoryTest, LoadFromFileSuccess) {
  gfx::ImageSkia image =
      gfx::ImageSkia(gfx::ImageSkiaRep(gfx::Size(20, 20), 0.0f));
  const SkBitmap* bitmap = image.bitmap();
  cc::WritePNGFile(*bitmap, GetPath(), /*discard_transparency=*/false);

  apps::mojom::IconValuePtr fallback_response, result;
  bool callback_called, fallback_called;
  RunLoadIconFromFileWithFallback(fallback_response.Clone(), &callback_called,
                                  &fallback_called, &result);
  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(fallback_called);
  EXPECT_FALSE(result.is_null());

  EXPECT_TRUE(
      cc::MatchesBitmap(*bitmap, *result->uncompressed.bitmap(),
                        cc::ExactPixelComparator(/*discard_alpha=*/false)));
}

TEST_F(AppIconFactoryTest, LoadFromFileFallback) {
  apps::mojom::IconValuePtr fallback_response = apps::mojom::IconValue::New();
  // Create a non-null image so we can check if we get the same image back.
  fallback_response->uncompressed =
      gfx::ImageSkia(gfx::ImageSkiaRep(gfx::Size(20, 20), 0.0f));

  apps::mojom::IconValuePtr result;
  bool callback_called, fallback_called;
  RunLoadIconFromFileWithFallback(fallback_response.Clone(), &callback_called,
                                  &fallback_called, &result);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(fallback_called);
  EXPECT_FALSE(result.is_null());
  EXPECT_TRUE(result->uncompressed.BackedBySameObjectAs(
      fallback_response->uncompressed));
}

TEST_F(AppIconFactoryTest, LoadFromFileFallbackFailure) {
  apps::mojom::IconValuePtr fallback_response, result;
  bool callback_called, fallback_called;
  RunLoadIconFromFileWithFallback(fallback_response.Clone(), &callback_called,
                                  &fallback_called, &result);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(fallback_called);
  EXPECT_FALSE(result.is_null());
  EXPECT_TRUE(fallback_response.is_null());
}

TEST_F(AppIconFactoryTest, LoadFromFileFallbackDoesNotReturn) {
  apps::mojom::IconValuePtr result;
  bool callback_called = false, fallback_called = false;

  apps::LoadIconFromFileWithFallback(
      apps::mojom::IconCompression::kUncompressed, 200, GetPath(),
      apps::IconEffects::kNone,
      base::BindOnce(
          [](bool* called, apps::mojom::IconValuePtr* result,
             base::OnceClosure quit, apps::mojom::IconValuePtr icon) {
            *called = true;
            *result = std::move(icon);
            std::move(quit).Run();
          },
          base::Unretained(&callback_called), base::Unretained(&result),
          run_loop_.QuitClosure()),
      base::BindOnce(
          [](bool* called, apps::mojom::Publisher::LoadIconCallback callback) {
            *called = true;
            // Drop the callback here, like a buggy fallback might.
          },
          base::Unretained(&fallback_called)));

  run_loop_.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(fallback_called);
  EXPECT_FALSE(result.is_null());
}
