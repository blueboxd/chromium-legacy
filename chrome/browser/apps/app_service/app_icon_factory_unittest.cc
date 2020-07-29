// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/arc/icon_decode_request.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#endif

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
        apps::mojom::IconType::kUncompressed, 200, GetPath(),
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

  std::string GetPngData(const std::string file_name) {
    base::FilePath base_path;
    std::string png_data_as_string;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &base_path));
    base::FilePath icon_file_path = base_path.AppendASCII("components")
                                        .AppendASCII("test")
                                        .AppendASCII("data")
                                        .AppendASCII("arc")
                                        .AppendASCII(file_name);
    CHECK(base::PathExists(icon_file_path));
    CHECK(base::ReadFileToString(icon_file_path, &png_data_as_string));
    return png_data_as_string;
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
      apps::mojom::IconType::kUncompressed, 200, GetPath(),
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

#if defined(OS_CHROMEOS)
TEST_F(AppIconFactoryTest, ArcNonAdaptiveIconToImageSkia) {
  arc::IconDecodeRequest::DisableSafeDecodingForTesting();
  std::string png_data_as_string = GetPngData("icon_100p.png");

  arc::mojom::RawIconPngDataPtr icon = arc::mojom::RawIconPngData::New(
      false,
      std::vector<uint8_t>(png_data_as_string.begin(),
                           png_data_as_string.end()),
      std::vector<uint8_t>(), std::vector<uint8_t>());

  bool callback_called = false;
  apps::ArcRawIconPngDataToImageSkia(
      std::move(icon), 100,
      base::BindOnce(
          [](bool* called, base::OnceClosure quit,
             const gfx::ImageSkia& image) {
            if (!image.isNull()) {
              *called = true;
            }
            std::move(quit).Run();
          },
          base::Unretained(&callback_called), run_loop_.QuitClosure()));

  run_loop_.Run();
  EXPECT_TRUE(callback_called);
}

TEST_F(AppIconFactoryTest, ArcAdaptiveIconToImageSkia) {
  arc::IconDecodeRequest::DisableSafeDecodingForTesting();
  std::string png_data_as_string = GetPngData("icon_100p.png");

  arc::mojom::RawIconPngDataPtr icon = arc::mojom::RawIconPngData::New(
      true, std::vector<uint8_t>(),
      std::vector<uint8_t>(png_data_as_string.begin(),
                           png_data_as_string.end()),
      std::vector<uint8_t>(png_data_as_string.begin(),
                           png_data_as_string.end()));

  bool callback_called = false;
  apps::ArcRawIconPngDataToImageSkia(
      std::move(icon), 100,
      base::BindOnce(
          [](bool* called, base::OnceClosure quit,
             const gfx::ImageSkia& image) {
            if (!image.isNull()) {
              *called = true;
            }
            std::move(quit).Run();
          },
          base::Unretained(&callback_called), run_loop_.QuitClosure()));

  run_loop_.Run();
  EXPECT_TRUE(callback_called);
}

TEST_F(AppIconFactoryTest, ArcActivityIconsToImageSkias) {
  arc::IconDecodeRequest::DisableSafeDecodingForTesting();
  std::string png_data_as_string = GetPngData("icon_100p.png");

  std::vector<arc::mojom::ActivityIconPtr> icons;
  icons.emplace_back(
      arc::mojom::ActivityIcon::New(arc::mojom::ActivityName::New("p0", "a0"),
                                    100, 100, std::vector<uint8_t>()));
  icons.emplace_back(arc::mojom::ActivityIcon::New(
      arc::mojom::ActivityName::New("p0", "a0"), 100, 100,
      std::vector<uint8_t>(),
      arc::mojom::RawIconPngData::New(
          false,
          std::vector<uint8_t>(png_data_as_string.begin(),
                               png_data_as_string.end()),
          std::vector<uint8_t>(), std::vector<uint8_t>())));
  icons.emplace_back(arc::mojom::ActivityIcon::New(
      arc::mojom::ActivityName::New("p0", "a0"), 201, 201,
      std::vector<uint8_t>(),
      arc::mojom::RawIconPngData::New(
          false,
          std::vector<uint8_t>(png_data_as_string.begin(),
                               png_data_as_string.end()),
          std::vector<uint8_t>(), std::vector<uint8_t>())));
  icons.emplace_back(arc::mojom::ActivityIcon::New(
      arc::mojom::ActivityName::New("p1", "a1"), 100, 100,
      std::vector<uint8_t>(),
      arc::mojom::RawIconPngData::New(
          true, std::vector<uint8_t>(),
          std::vector<uint8_t>(png_data_as_string.begin(),
                               png_data_as_string.end()),
          std::vector<uint8_t>(png_data_as_string.begin(),
                               png_data_as_string.end()))));

  std::vector<gfx::ImageSkia> result;
  bool callback_called = false;
  apps::ArcActivityIconsToImageSkias(
      icons, base::BindOnce(
                 [](bool* called, std::vector<gfx::ImageSkia>* result,
                    base::OnceClosure quit,
                    const std::vector<gfx::ImageSkia>& images) {
                   *called = true;
                   for (auto image : images) {
                     result->emplace_back(image);
                   }
                   std::move(quit).Run();
                 },
                 base::Unretained(&callback_called), base::Unretained(&result),
                 run_loop_.QuitClosure()));
  run_loop_.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(4U, result.size());
  if (result.size() == 4U) {
    EXPECT_TRUE(result[0].isNull());
    EXPECT_FALSE(result[1].isNull());
    EXPECT_TRUE(result[2].isNull());
    EXPECT_FALSE(result[3].isNull());
  }
}
#endif
