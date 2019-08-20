// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/notifications/scheduler/internal/icon_converter_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifications {
namespace {

class IconConverterTest : public testing::Test {
 public:
  IconConverterTest() {}
  ~IconConverterTest() override = default;

  void SetUp() override {
    auto icon_converter = std::make_unique<IconConverterImpl>();
    icon_converter_ = std::move(icon_converter);
    encoded_data_.clear();
    decoded_icons_.clear();
  }

  void OnIconsEncoded(base::OnceClosure quit_closure,
                      std::vector<std::string> encoded_data) {
    encoded_data_ = std::move(encoded_data);
    std::move(quit_closure).Run();
  }

  void OnIconsDecoded(base::OnceClosure quit_closure,
                      std::vector<SkBitmap> decoded_icons) {
    decoded_icons_ = std::move(decoded_icons);
    std::move(quit_closure).Run();
  }

  void VerifyEncodeRoundTrip(std::vector<SkBitmap> input) {
    EXPECT_EQ(decoded_icons_.size(), input.size());
    for (size_t i = 0; i < input.size(); i++) {
      EXPECT_EQ(input[i].height(), decoded_icons_[i].height());
      EXPECT_EQ(input[i].width(), decoded_icons_[i].width());
    }
  }

 protected:
  IconConverter* icon_converter() { return icon_converter_.get(); }
  std::vector<SkBitmap>* decoded_icons() { return &decoded_icons_; }
  std::vector<std::string>* encoded_data() { return &encoded_data_; }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<IconConverter> icon_converter_;
  std::vector<std::string> encoded_data_;
  std::vector<SkBitmap> decoded_icons_;
  DISALLOW_COPY_AND_ASSIGN(IconConverterTest);
};

TEST_F(IconConverterTest, EncodeRoundTrip) {
  SkBitmap icon1, icon2;
  icon1.allocN32Pixels(3, 4);
  icon2.allocN32Pixels(12, 13);
  std::vector<SkBitmap> input = {std::move(icon1), std::move(icon2)};
  {
    base::RunLoop run_loop;
    icon_converter()->ConvertIconToString(
        input, base::BindOnce(&IconConverterTest::OnIconsEncoded,
                              base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_EQ(encoded_data()->size(), input.size());
  {
    base::RunLoop run_loop;
    icon_converter()->ConvertStringToIcon(
        *encoded_data(),
        base::BindOnce(&IconConverterTest::OnIconsDecoded,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    VerifyEncodeRoundTrip(input);
  }
}

}  // namespace
}  // namespace notifications
