// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_H_

#include <deque>
#include <vector>

#include "ash/ash_export.h"
#include "base/component_export.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_observer.h"

namespace ash {

// Keeps track of the last few things saved in the clipboard.
class ASH_EXPORT ClipboardHistory : public ui::ClipboardObserver {
 public:
  // Prevents clipboard history from recording history within its scope. If
  // anything is copied within its scope, history will not be recorded.
  class ASH_EXPORT ScopedPause {
   public:
    explicit ScopedPause(ClipboardHistory* clipboard_history);
    ScopedPause(const ScopedPause&) = delete;
    ScopedPause& operator=(const ScopedPause&) = delete;
    ~ScopedPause();

   private:
    ClipboardHistory* const clipboard_history_;
  };

  ClipboardHistory();
  ClipboardHistory(const ClipboardHistory&) = delete;
  ClipboardHistory& operator=(const ClipboardHistory&) = delete;
  ~ClipboardHistory() override;

  // Deletes content from |history_with_duplicates_|. Does not modify content
  // stored in the clipboard.
  void ClearHistory();

  std::vector<ui::ClipboardData> GetRecentClipboardDataWithNoDuplicates() const;

  // Whether there is no history recorded.
  bool IsEmpty() const;

  // ClipboardMonitor:
  void OnClipboardDataChanged() override;

 private:
  void PauseClipboardHistory();
  void UnPauseClipboardHistory();

  // The count of clipboard history pauses.
  size_t num_pause_clipboard_history_ = 0;
  // History of the most recent things copied. Duplicates are kept to keep the
  // most recent ordering.
  std::deque<ui::ClipboardData> history_with_duplicates_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_H_