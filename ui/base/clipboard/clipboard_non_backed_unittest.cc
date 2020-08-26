// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_non_backed.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_data.h"

namespace ui {

class ClipboardNonBackedTest : public testing::Test {
 public:
  ClipboardNonBackedTest() = default;
  ClipboardNonBackedTest(const ClipboardNonBackedTest&) = delete;
  ClipboardNonBackedTest& operator=(const ClipboardNonBackedTest&) = delete;
  ~ClipboardNonBackedTest() override = default;

  ClipboardNonBacked* clipboard() { return &clipboard_; }

 private:
  ClipboardNonBacked clipboard_;
};

// Verifies that GetClipboardData() returns the same instance of ClipboardData
// as was written via WriteClipboardData().
TEST_F(ClipboardNonBackedTest, WriteAndGetClipboardData) {
  auto clipboard_data = std::make_unique<ClipboardData>();

  auto* expected_clipboard_data_ptr = clipboard_data.get();
  clipboard()->WriteClipboardData(std::move(clipboard_data));
  auto* actual_clipboard_data_ptr = clipboard()->GetClipboardData(nullptr);

  EXPECT_EQ(expected_clipboard_data_ptr, actual_clipboard_data_ptr);
}

// Verifies that WriteClipboardData() writes a ClipboardData instance to the
// clipboard and returns the previous instance.
TEST_F(ClipboardNonBackedTest, WriteClipboardData) {
  auto first_data = std::make_unique<ClipboardData>();
  auto second_data = std::make_unique<ClipboardData>();

  auto* first_data_ptr = first_data.get();
  auto* second_data_ptr = second_data.get();

  auto previous_data = clipboard()->WriteClipboardData(std::move(first_data));
  EXPECT_EQ(previous_data.get(), nullptr);

  previous_data = clipboard()->WriteClipboardData(std::move(second_data));

  EXPECT_EQ(first_data_ptr, previous_data.get());
  EXPECT_EQ(second_data_ptr, clipboard()->GetClipboardData(nullptr));
}

// Verifies that directly writing to ClipboardInternal does not result in
// histograms being logged. This is used by ClipboardHistoryController to
// manipulate the clipboard in order to facilitate pasting from clipboard
// history.
TEST_F(ClipboardNonBackedTest, AdminWriteDoesNotRecordHistograms) {
  base::HistogramTester histogram_tester;
  auto data = std::make_unique<ClipboardData>();
  data->set_text("test");

  auto* data_ptr = data.get();

  // Write the data to the clipboard, no histograms should be recorded.
  clipboard()->WriteClipboardData(std::move(data));
  EXPECT_EQ(data_ptr, clipboard()->GetClipboardData(/*data_dst=*/nullptr));

  histogram_tester.ExpectTotalCount("Clipboard.Read", 0);
  histogram_tester.ExpectTotalCount("Clipboard.Write", 0);
}

}  // namespace ui
