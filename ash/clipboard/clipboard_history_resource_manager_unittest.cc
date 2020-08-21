// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_resource_manager.h"

#include <string>
#include <unordered_map>

#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_controller.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

namespace {

void FlushMessageLoop() {
  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   run_loop.QuitClosure());
  run_loop.Run();
}

SkBitmap GetRandomBitmap() {
  SkColor color = rand() % 0xFFFFFF + 1;

  SkBitmap bitmap;
  bitmap.allocN32Pixels(24, 24);
  bitmap.eraseARGB(255, SkColorGetR(color), SkColorGetG(color),
                   SkColorGetB(color));
  return bitmap;
}

ui::ImageModel GetRandomImageModel() {
  return ui::ImageModel::FromImageSkia(
      gfx::ImageSkia::CreateFrom1xBitmap(GetRandomBitmap()));
}

// ClipboardHistoryItemBuilder ------------------------------------------------

class ClipboardHistoryItemBuilder {
 public:
  ClipboardHistoryItemBuilder() = default;
  ClipboardHistoryItemBuilder(const ClipboardHistoryItemBuilder&) = delete;
  ClipboardHistoryItemBuilder& operator=(const ClipboardHistoryItemBuilder&) =
      delete;
  ~ClipboardHistoryItemBuilder() = default;

  ClipboardHistoryItem Build() const {
    ui::ClipboardData data;
    if (text_.has_value())
      data.set_text(text_.value());
    if (markup_.has_value())
      data.set_markup_data(markup_.value());
    if (rtf_.has_value())
      data.SetRTFData(rtf_.value());
    if (bookmark_title_.has_value())
      data.set_bookmark_title(bookmark_title_.value());
    if (bitmap_.has_value())
      data.SetBitmapData(bitmap_.value());
    if (custom_format_.has_value() && custom_data_.has_value())
      data.SetCustomData(custom_format_.value(), custom_data_.value());
    if (web_smart_paste_.has_value())
      data.set_web_smart_paste(web_smart_paste_.value());
    return ClipboardHistoryItem(std::move(data));
  }

  ClipboardHistoryItemBuilder& SetText(const std::string& text) {
    text_ = text;
    return *this;
  }

  ClipboardHistoryItemBuilder& ClearText() {
    text_ = base::nullopt;
    return *this;
  }

  ClipboardHistoryItemBuilder& SetMarkup(const std::string& markup) {
    markup_ = markup;
    return *this;
  }

  ClipboardHistoryItemBuilder& ClearMarkup() {
    markup_ = base::nullopt;
    return *this;
  }

  ClipboardHistoryItemBuilder& SetRtf(const std::string& rtf) {
    rtf_ = rtf;
    return *this;
  }

  ClipboardHistoryItemBuilder& ClearRtf() {
    rtf_ = base::nullopt;
    return *this;
  }

  ClipboardHistoryItemBuilder& SetBookmarkTitle(
      const std::string& bookmark_title) {
    bookmark_title_ = bookmark_title;
    return *this;
  }

  ClipboardHistoryItemBuilder& ClearBookmarkTitle() {
    bookmark_title_ = base::nullopt;
    return *this;
  }

  ClipboardHistoryItemBuilder& SetBitmap(const SkBitmap& bitmap) {
    bitmap_ = bitmap;
    return *this;
  }

  ClipboardHistoryItemBuilder& ClearBitmap() {
    bitmap_ = base::nullopt;
    return *this;
  }

  ClipboardHistoryItemBuilder& SetCustomData(const std::string& custom_format,
                                             const std::string& custom_data) {
    custom_format_ = custom_format;
    custom_data_ = custom_data;
    return *this;
  }

  ClipboardHistoryItemBuilder& ClearCustomData() {
    custom_format_ = base::nullopt;
    custom_data_ = base::nullopt;
    return *this;
  }

  ClipboardHistoryItemBuilder& SetFileSystemData(
      std::initializer_list<std::string>&& source_list) {
    constexpr char kFileSystemSourcesType[] = "fs/sources";

    base::Pickle custom_data;
    ui::WriteCustomDataToPickle(
        std::unordered_map<base::string16, base::string16>(
            {{base::UTF8ToUTF16(kFileSystemSourcesType),
              base::UTF8ToUTF16(base::JoinString(source_list, "\n"))}}),
        &custom_data);

    return SetCustomData(
        ui::ClipboardFormatType::GetWebCustomDataType().GetName(),
        std::string(static_cast<const char*>(custom_data.data()),
                    custom_data.size()));
  }

  ClipboardHistoryItemBuilder& SetWebSmartPaste(bool web_smart_paste) {
    web_smart_paste_ = web_smart_paste;
    return *this;
  }

  ClipboardHistoryItemBuilder& ClearWebSmartPaste() {
    web_smart_paste_ = base::nullopt;
    return *this;
  }

 private:
  base::Optional<std::string> text_;
  base::Optional<std::string> markup_;
  base::Optional<std::string> rtf_;
  base::Optional<std::string> bookmark_title_;
  base::Optional<SkBitmap> bitmap_;
  base::Optional<std::string> custom_format_;
  base::Optional<std::string> custom_data_;
  base::Optional<bool> web_smart_paste_;
};

}  // namespace

// Tests -----------------------------------------------------------------------

class MockClipboardImageModelFactory : public ClipboardImageModelFactory {
 public:
  MockClipboardImageModelFactory() = default;
  MockClipboardImageModelFactory(const MockClipboardImageModelFactory&) =
      delete;
  MockClipboardImageModelFactory& operator=(
      const MockClipboardImageModelFactory&) = delete;
  ~MockClipboardImageModelFactory() override = default;

  MOCK_METHOD(void,
              Render,
              (const base::UnguessableToken&,
               const std::string&,
               ImageModelCallback),
              (override));
  MOCK_METHOD(void, CancelRequest, (const base::UnguessableToken&), (override));
  MOCK_METHOD(void, Activate, (), (override));
  MOCK_METHOD(void, Deactivate, (), (override));
};

class ClipboardHistoryResourceManagerTest : public AshTestBase {
 public:
  ClipboardHistoryResourceManagerTest() = default;
  ClipboardHistoryResourceManagerTest(
      const ClipboardHistoryResourceManagerTest&) = delete;
  ClipboardHistoryResourceManagerTest& operator=(
      const ClipboardHistoryResourceManagerTest&) = delete;
  ~ClipboardHistoryResourceManagerTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kClipboardHistory);
    AshTestBase::SetUp();
    clipboard_history_ =
        Shell::Get()->clipboard_history_controller()->history();
    resource_manager_ =
        Shell::Get()->clipboard_history_controller()->resource_manager();
    mock_image_factory_ =
        std::make_unique<testing::StrictMock<MockClipboardImageModelFactory>>();
  }

  const ClipboardHistory* clipboard_history() const {
    return clipboard_history_;
  }

  const ClipboardHistoryResourceManager* resource_manager() {
    return resource_manager_;
  }

  MockClipboardImageModelFactory* mock_image_factory() {
    return mock_image_factory_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  const ClipboardHistory* clipboard_history_;
  const ClipboardHistoryResourceManager* resource_manager_;
  std::unique_ptr<MockClipboardImageModelFactory> mock_image_factory_;
};

TEST_F(ClipboardHistoryResourceManagerTest, GetLabel) {
  base::test::ScopedRestoreICUDefaultLocale locale("en_US");

  // Populate a builder with all the data formats that we expect to handle.
  ClipboardHistoryItemBuilder builder;
  builder.SetText("Text")
      .SetMarkup("Markup")
      .SetRtf("Rtf")
      .SetBookmarkTitle("Bookmark Title")
      .SetBitmap(gfx::test::CreateBitmap(10, 10))
      .SetCustomData("Custom Format", "Custom Data")
      .SetWebSmartPaste(true);

  // Bitmap data always take precedence.
  EXPECT_EQ(resource_manager()->GetLabel(builder.Build()),
            base::UTF8ToUTF16("Image"));

  builder.ClearBitmap();

  // In the absence of bitmap data, text data takes precedence.
  EXPECT_EQ(resource_manager()->GetLabel(builder.Build()),
            base::UTF8ToUTF16("Text"));

  builder.ClearText();

  // In the absence of text data, HTML data takes precedence.
  EXPECT_EQ(resource_manager()->GetLabel(builder.Build()),
            base::UTF8ToUTF16("Markup"));

  builder.ClearMarkup();

  // In the absence of HTML data, RTF data takes precedence.
  EXPECT_EQ(resource_manager()->GetLabel(builder.Build()),
            base::UTF8ToUTF16("RTF Content"));

  builder.ClearRtf();

  // In the absence of RTF data, bookmark data takes precedence.
  EXPECT_EQ(resource_manager()->GetLabel(builder.Build()),
            base::UTF8ToUTF16("Bookmark Title"));

  builder.ClearBookmarkTitle();

  // In the absence of bookmark data, web smart paste data takes precedence.
  EXPECT_EQ(resource_manager()->GetLabel(builder.Build()),
            base::UTF8ToUTF16("Web Smart Paste Content"));

  builder.ClearWebSmartPaste();

  // In the absence of web smart paste data, custom data takes precedence.
  EXPECT_EQ(resource_manager()->GetLabel(builder.Build()),
            base::UTF8ToUTF16("<Custom Data>"));

  builder.SetFileSystemData(
      {"/path/to/My%20File.txt", "/path/to/My%20Other%20File.txt"});

  // We specially treat custom file system data to show a list of file names.
  EXPECT_EQ(resource_manager()->GetLabel(builder.Build()),
            base::UTF8ToUTF16("My File.txt, My Other File.txt"));
}

// Tests that Render is called once when an eligible item is added
// to ClipboardHistory.
TEST_F(ClipboardHistoryResourceManagerTest, BasicCachedImageModel) {
  ui::ImageModel expected_image_model = GetRandomImageModel();
  ON_CALL(*mock_image_factory(), Render)
      .WillByDefault(testing::WithArg<2>(
          [&](ClipboardImageModelFactory::ImageModelCallback callback) {
            std::move(callback).Run(expected_image_model);
          }));
  EXPECT_CALL(*mock_image_factory(), CancelRequest).Times(0);
  EXPECT_CALL(*mock_image_factory(), Render).Times(1);

  // Write a basic ClipboardData which is eligible to render HTML.
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(base::UTF8ToUTF16("test"), "source_url");
  }

  FlushMessageLoop();

  EXPECT_EQ(expected_image_model, resource_manager()->GetImageModel(
                                      clipboard_history()->GetItems().front()));
}

// Tests that copying duplicate HTML to the buffer results in only one render
// request, and that that request is canceled once when the item is forgotten.
TEST_F(ClipboardHistoryResourceManagerTest, DuplicateHTML) {
  // Write two duplicate ClipboardDatas. Two things should be in clipboard
  // history, but they should share a CachedImageModel.
  ui::ImageModel expected_image_model = GetRandomImageModel();
  ON_CALL(*mock_image_factory(), Render)
      .WillByDefault(testing::WithArg<2>(
          [&](ClipboardImageModelFactory::ImageModelCallback callback) {
            std::move(callback).Run(expected_image_model);
          }));
  EXPECT_CALL(*mock_image_factory(), CancelRequest).Times(0);
  EXPECT_CALL(*mock_image_factory(), Render).Times(1);

  for (int i = 0; i < 2; ++i) {
    {
      ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
      scw.WriteHTML(base::UTF8ToUTF16("test"), "source_url");
    }
    FlushMessageLoop();
  }
  auto items = clipboard_history()->GetItems();
  EXPECT_EQ(2u, items.size());
  for (const auto& item : items)
    EXPECT_EQ(expected_image_model, resource_manager()->GetImageModel(item));
}

// Tests that two different eligible ClipboardData copied results in two calls
// to Render and Cancel.
TEST_F(ClipboardHistoryResourceManagerTest, DifferentHTML) {
  // Write two ClipboardData with different HTML.
  ui::ImageModel first_expected_image_model = GetRandomImageModel();
  ui::ImageModel second_expected_image_model = GetRandomImageModel();
  std::deque<ui::ImageModel> expected_image_models{first_expected_image_model,
                                                   second_expected_image_model};
  ON_CALL(*mock_image_factory(), Render)
      .WillByDefault(testing::WithArg<2>(
          [&](ClipboardImageModelFactory::ImageModelCallback callback) {
            std::move(callback).Run(expected_image_models.front());
            expected_image_models.pop_front();
          }));
  EXPECT_CALL(*mock_image_factory(), Render).Times(2);
  EXPECT_CALL(*mock_image_factory(), CancelRequest).Times(0);
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(base::UTF8ToUTF16("test"), "source_url");
  }
  FlushMessageLoop();
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(base::UTF8ToUTF16("different"), "source_url");
  }
  FlushMessageLoop();

  std::list<ClipboardHistoryItem> items = clipboard_history()->GetItems();
  EXPECT_EQ(2u, items.size());
  EXPECT_EQ(second_expected_image_model,
            resource_manager()->GetImageModel(items.front()));
  items.pop_front();
  EXPECT_EQ(first_expected_image_model,
            resource_manager()->GetImageModel(items.front()));
}

// Tests that items that are ineligible for CachedImageModels (items with image
// representations, or no markup) do not request Render.
TEST_F(ClipboardHistoryResourceManagerTest, IneligibleItem) {
  // Write a ClipboardData with an image, no CachedImageModel should be created.
  EXPECT_CALL(*mock_image_factory(), Render).Times(0);
  EXPECT_CALL(*mock_image_factory(), CancelRequest).Times(0);
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(base::UTF8ToUTF16("test"), "source_url");
    scw.WriteImage(GetRandomBitmap());
  }
  FlushMessageLoop();

  EXPECT_EQ(1u, clipboard_history()->GetItems().size());

  // Write a ClipboardData with no markup and no image. No CachedImageModel
  // should be created.
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);

    scw.WriteText(base::UTF8ToUTF16("test"));

    scw.WriteRTF("rtf");

    scw.WriteBookmark(base::UTF8ToUTF16("bookmark_title"), "test_url");
  }
  FlushMessageLoop();

  EXPECT_EQ(2u, clipboard_history()->GetItems().size());
}

// Tests that incomplete requests are canceled when the item corresponding with
// the request is forgotten by ClipboardHistory.
TEST_F(ClipboardHistoryResourceManagerTest, IncompleteRequestCanceled) {
  EXPECT_CALL(*mock_image_factory(), Render).Times(1);
  EXPECT_CALL(*mock_image_factory(), CancelRequest).Times(1);
  // Because we do not provide an ON_CALL for MockClipboardImageModelFactory,
  // Render will do nothing. This simulates an incomplete request from the
  // perspective of ClipboardHistoryResourceManager.
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(base::UTF8ToUTF16("test"), "source_url");
  }
  FlushMessageLoop();
}

}  // namespace ash
