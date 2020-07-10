// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history.h"

#include "ash/clipboard/multipaste_controller.h"
#include "ash/shell.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_monitor.h"

namespace ash {

namespace {

constexpr size_t kMaxClipboardItemsShared = 5;

}  // namespace

ClipboardHistory::ScopedPause::ScopedPause(ClipboardHistory* clipboard_history)
    : clipboard_history_(clipboard_history) {
  clipboard_history_->PauseClipboardHistory();
}

ClipboardHistory::ScopedPause::~ScopedPause() {
  clipboard_history_->UnPauseClipboardHistory();
}

ClipboardHistory::ClipboardHistory() {
  ui::ClipboardMonitor::GetInstance()->AddObserver(this);
}

ClipboardHistory::~ClipboardHistory() {
  ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);
}

void ClipboardHistory::ClearHistory() {
  history_with_duplicates_ = std::deque<ui::ClipboardData>();
}

std::vector<ui::ClipboardData>
ClipboardHistory::GetRecentClipboardDataWithNoDuplicates() const {
  std::vector<ui::ClipboardData> history_without_duplicates;
  for (auto it = history_with_duplicates_.crbegin();
       it != history_with_duplicates_.crend(); ++it) {
    if (history_without_duplicates.size() == kMaxClipboardItemsShared)
      break;
    const ui::ClipboardData& current_data = *it;
    if (!base::Contains(history_without_duplicates, current_data))
      history_without_duplicates.push_back(current_data);
  }
  return history_without_duplicates;
}

bool ClipboardHistory::IsEmpty() const {
  return history_with_duplicates_.empty();
}

void ClipboardHistory::OnClipboardDataChanged() {
  // TODO(newcomer): Prevent Clipboard from recording metrics when pausing
  // observation.
  if (num_pause_clipboard_history_ > 0)
    return;

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  CHECK(clipboard);

  std::vector<base::string16> mime_types;
  clipboard->ReadAvailableTypes(ui::ClipboardBuffer::kCopyPaste, &mime_types);

  ui::ClipboardData new_data;
  for (const auto& type16 : mime_types) {
    const std::string type(base::UTF16ToUTF8(type16));
    if (type == ui::ClipboardFormatType::GetPlainTextType().GetName()) {
      base::string16 text;
      clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, &text);
      new_data.set_text(base::UTF16ToUTF8(text));
    } else if (type == ui::ClipboardFormatType::GetHtmlType().GetName()) {
      uint32_t start, end;
      base::string16 html_markup;
      std::string src_url;
      clipboard->ReadHTML(ui::ClipboardBuffer::kCopyPaste, &html_markup,
                          &src_url, &start, &end);
      new_data.set_markup_data(base::UTF16ToUTF8(html_markup));
      new_data.set_url(src_url);
    } else if (type == ui::ClipboardFormatType::GetRtfType().GetName()) {
      std::string rtf_data;
      clipboard->ReadRTF(ui::ClipboardBuffer::kCopyPaste, &rtf_data);
      new_data.SetRTFData(rtf_data);
    } else if (type == ui::ClipboardFormatType::GetBitmapType().GetName()) {
      // TODO(newcomer): Read the bitmap asynchronously.
    }
    // TODO(newcomer): Handle custom data, which could be files.
  }
  // Keep duplicates to maintain most recent ordering.
  history_with_duplicates_.push_back(std::move(new_data));

  // Keep the history with duplicates to a reasonable limit, but greater than
  // kMaxClipboardItemsShared, so that repeated copies to not push off relevant
  // data.
  if (history_with_duplicates_.size() > kMaxClipboardItemsShared * 2)
    history_with_duplicates_.pop_front();
}

void ClipboardHistory::PauseClipboardHistory() {
  ++num_pause_clipboard_history_;
}

void ClipboardHistory::UnPauseClipboardHistory() {
  --num_pause_clipboard_history_;
}

}  // namespace ash