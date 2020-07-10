// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history.h"

#include "ash/clipboard/multipaste_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace ash {

class ClipboardHistoryTest : public AshTestBase {
 public:
  ClipboardHistoryTest() = default;
  ClipboardHistoryTest(const ClipboardHistoryTest&) = delete;
  ClipboardHistoryTest& operator=(const ClipboardHistoryTest&) = delete;
  ~ClipboardHistoryTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({chromeos::features::kMultipaste},
                                          {});
    AshTestBase::SetUp();
    clipboard_history_ =
        Shell::Get()->multipaste_controller()->clipboard_history();
  }

  std::vector<ui::ClipboardData> GetClipboardHistoryData() {
    return clipboard_history_->GetRecentClipboardDataWithNoDuplicates();
  }

  void WriteAndEnsureTextHistory(
      const std::vector<base::string16>& input_strings,
      const std::vector<base::string16>& expected_strings) {
    for (const auto& input_string : input_strings) {
      ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
      scw.WriteText(input_string);
    }
    EnsureTextHistory(expected_strings);
  }

  void EnsureTextHistory(const std::vector<base::string16>& expected_strings) {
    const std::vector<ui::ClipboardData> datas = GetClipboardHistoryData();
    EXPECT_EQ(expected_strings.size(), datas.size());

    int expected_strings_index = 0;
    for (auto& data : datas) {
      EXPECT_EQ(expected_strings[expected_strings_index++],
                base::UTF8ToUTF16(data.text()));
    }
  }

  ClipboardHistory* clipboard_history() { return clipboard_history_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  // Owned by MultipasteController.
  ClipboardHistory* clipboard_history_ = nullptr;
};

// Tests that with nothing copied, nothing is shown.
TEST_F(ClipboardHistoryTest, NothingCopiedNothingSaved) {
  // When nothing is copied, nothing should be saved.
  WriteAndEnsureTextHistory(/*input_strings=*/{},
                            /*expected_strings=*/{});
}

// Tests that if one thing is copied, one thing is saved.
TEST_F(ClipboardHistoryTest, OneThingCopiedOneThingSaved) {
  std::vector<base::string16> input_strings{base::UTF8ToUTF16("test")};
  std::vector<base::string16> expected_strings = input_strings;

  // Test that only one string is in history.
  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests that if the same (non bitmap) thing is copied, only one thing is saved.
TEST_F(ClipboardHistoryTest, DuplicateBasic) {
  std::vector<base::string16> input_strings{base::UTF8ToUTF16("test"),
                                            base::UTF8ToUTF16("test")};
  // Because |input_strings| is the same string twice, history should detect the
  // duplicate.
  std::vector<base::string16> expected_strings{base::UTF8ToUTF16("test")};

  // Test that only one thing is saved.
  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests the ordering of history is in reverse chronological order.
TEST_F(ClipboardHistoryTest, HistoryIsReverseChronological) {
  std::vector<base::string16> input_strings{
      base::UTF8ToUTF16("test1"), base::UTF8ToUTF16("test2"),
      base::UTF8ToUTF16("test3"), base::UTF8ToUTF16("test4")};
  std::vector<base::string16> expected_strings = input_strings;
  // Reverse the vector, history should match this ordering.
  std::reverse(std::begin(expected_strings), std::end(expected_strings));
  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests that when a duplicate is copied, the duplicate shows up in the proper
// order, and the older version is not returned.
TEST_F(ClipboardHistoryTest, DuplicateOverwritesPreviousRecord) {
  // Input holds a unique string sandwiched by a copy.
  std::vector<base::string16> input_strings{base::UTF8ToUTF16("test1"),
                                            base::UTF8ToUTF16("test2"),
                                            base::UTF8ToUTF16("test1")};
  // The result should be a reversal of the last two elements. When a duplicate
  // is copied, history will show the most recent version of that duplicate.
  std::vector<base::string16> expected_strings{base::UTF8ToUTF16("test1"),
                                               base::UTF8ToUTF16("test2")};

  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests that nothing is saved after history is cleared.
TEST_F(ClipboardHistoryTest, ClearHistoryBasic) {
  // Input holds a unique string sandwhiched by a copy.
  std::vector<base::string16> input_strings{base::UTF8ToUTF16("test1"),
                                            base::UTF8ToUTF16("test2"),
                                            base::UTF8ToUTF16("test1")};
  // The result should be a reversal of the last two elements. When a duplicate
  // is copied, history will show the most recent version of that duplicate.
  std::vector<base::string16> expected_strings{};

  for (const auto& input_string : input_strings) {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteText(input_string);
  }

  clipboard_history()->ClearHistory();
  EnsureTextHistory(expected_strings);
}

// Tests that the limit of clipboard history is respected.
TEST_F(ClipboardHistoryTest, HistoryLimit) {
  std::vector<base::string16> input_strings{
      base::UTF8ToUTF16("test1"), base::UTF8ToUTF16("test2"),
      base::UTF8ToUTF16("test3"), base::UTF8ToUTF16("test4"),
      base::UTF8ToUTF16("test5"), base::UTF8ToUTF16("test6")};

  // The result should be a reversal of the last five elements.
  std::vector<base::string16> expected_strings{input_strings.begin() + 1,
                                               input_strings.end()};
  std::reverse(expected_strings.begin(), expected_strings.end());
  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests that pausing clipboard history results in no history collected.
TEST_F(ClipboardHistoryTest, PauseHistory) {
  std::vector<base::string16> input_strings{base::UTF8ToUTF16("test1"),
                                            base::UTF8ToUTF16("test2"),
                                            base::UTF8ToUTF16("test1")};
  // Because history is paused, there should be nothing stored.
  std::vector<base::string16> expected_strings{};

  ClipboardHistory::ScopedPause scoped_pause(clipboard_history());
  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

}  // namespace ash