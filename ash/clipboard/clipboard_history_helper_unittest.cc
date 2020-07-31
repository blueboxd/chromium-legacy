// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_helper.h"

#include <string>
#include <unordered_map>

#include "base/optional.h"
#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {
namespace clipboard {
namespace helper {

namespace {

// ClipboardDataBuilder --------------------------------------------------------

class ClipboardDataBuilder {
 public:
  ClipboardDataBuilder() = default;
  ClipboardDataBuilder(const ClipboardDataBuilder&) = delete;
  ClipboardDataBuilder& operator=(const ClipboardDataBuilder&) = delete;
  ~ClipboardDataBuilder() = default;

  ui::ClipboardData Build() const {
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
    return data;
  }

  ClipboardDataBuilder& SetText(const std::string& text) {
    text_ = text;
    return *this;
  }

  ClipboardDataBuilder& ClearText() {
    text_ = base::nullopt;
    return *this;
  }

  ClipboardDataBuilder& SetMarkup(const std::string& markup) {
    markup_ = markup;
    return *this;
  }

  ClipboardDataBuilder& ClearMarkup() {
    markup_ = base::nullopt;
    return *this;
  }

  ClipboardDataBuilder& SetRtf(const std::string& rtf) {
    rtf_ = rtf;
    return *this;
  }

  ClipboardDataBuilder& ClearRtf() {
    rtf_ = base::nullopt;
    return *this;
  }

  ClipboardDataBuilder& SetBookmarkTitle(const std::string& bookmark_title) {
    bookmark_title_ = bookmark_title;
    return *this;
  }

  ClipboardDataBuilder& ClearBookmarkTitle() {
    bookmark_title_ = base::nullopt;
    return *this;
  }

  ClipboardDataBuilder& SetBitmap(const SkBitmap& bitmap) {
    bitmap_ = bitmap;
    return *this;
  }

  ClipboardDataBuilder& ClearBitmap() {
    bitmap_ = base::nullopt;
    return *this;
  }

  ClipboardDataBuilder& SetCustomData(const std::string& custom_format,
                                      const std::string& custom_data) {
    custom_format_ = custom_format;
    custom_data_ = custom_data;
    return *this;
  }

  ClipboardDataBuilder& ClearCustomData() {
    custom_format_ = base::nullopt;
    custom_data_ = base::nullopt;
    return *this;
  }

  ClipboardDataBuilder& SetFileSystemData(
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

  ClipboardDataBuilder& SetWebSmartPaste(bool web_smart_paste) {
    web_smart_paste_ = web_smart_paste;
    return *this;
  }

  ClipboardDataBuilder& ClearWebSmartPaste() {
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

using ClipboardHistoryHelperTest = testing::Test;

TEST_F(ClipboardHistoryHelperTest, GetLabel) {
  base::test::ScopedRestoreICUDefaultLocale locale("en_US");

  // Populate a builder with all the data formats that we expect to handle.
  ClipboardDataBuilder builder;
  builder.SetText("Text")
      .SetMarkup("Markup")
      .SetRtf("Rtf")
      .SetBookmarkTitle("Bookmark Title")
      .SetBitmap(gfx::test::CreateBitmap(10, 10))
      .SetCustomData("Custom Format", "Custom Data")
      .SetWebSmartPaste(true);

  // Bitmap data always take precedence.
  EXPECT_EQ(GetLabel(builder.Build()), base::UTF8ToUTF16("Image"));

  builder.ClearBitmap();

  // In the absence of bitmap data, text data takes precedence.
  EXPECT_EQ(GetLabel(builder.Build()), base::UTF8ToUTF16("Text"));

  builder.ClearText();

  // In the absence of text data, HTML data takes precedence.
  EXPECT_EQ(GetLabel(builder.Build()), base::UTF8ToUTF16("Markup"));

  builder.ClearMarkup();

  // In the absence of HTML data, RTF data takes precedence.
  EXPECT_EQ(GetLabel(builder.Build()), base::UTF8ToUTF16("RTF Content"));

  builder.ClearRtf();

  // In the absence of RTF data, bookmark data takes precedence.
  EXPECT_EQ(GetLabel(builder.Build()), base::UTF8ToUTF16("Bookmark Title"));

  builder.ClearBookmarkTitle();

  // In the absence of bookmark data, web smart paste data takes precedence.
  EXPECT_EQ(GetLabel(builder.Build()),
            base::UTF8ToUTF16("Web Smart Paste Content"));

  builder.ClearWebSmartPaste();

  // In the absence of web smart paste data, custom data takes precedence.
  EXPECT_EQ(GetLabel(builder.Build()), base::UTF8ToUTF16("<Custom Data>"));

  builder.SetFileSystemData(
      {"/path/to/My%20File.txt", "/path/to/My%20Other%20File.txt"});

  // We specially treat custom file system data to show a list of file names.
  EXPECT_EQ(GetLabel(builder.Build()),
            base::UTF8ToUTF16("My File.txt, My Other File.txt"));
}

}  // namespace helper
}  // namespace clipboard
}  // namespace ash
