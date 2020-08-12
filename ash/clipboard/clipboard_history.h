// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_H_

#include <list>
#include <map>

#include "ash/ash_export.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_observer.h"

namespace ui {
class ClipboardData;
}  // namespace ui

namespace ash {

// Keeps track of the last few things saved in the clipboard.
class ASH_EXPORT ClipboardHistory : public ui::ClipboardObserver {
 public:
  // Prevents clipboard history from being recorded within its scope. If
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

  // Returns the list of most recent items. The returned list is sorted by
  // recency.
  const std::list<ui::ClipboardData>& GetItems() const;

  // Deletes clipboard history. Does not modify content stored in the clipboard.
  void Clear();

  // Returns whether the clipboard history of the active account is empty.
  bool IsEmpty() const;

  // ClipboardMonitor:
  void OnClipboardDataChanged() override;

 private:
  // Adds |data| to the |history_list_|.
  void CommitData(ui::ClipboardData data);

  void Pause();
  void Resume();

  // The count of pauses.
  size_t num_pause_ = 0;

  // The history of data copied to the Clipboard. Items of the list are sorted
  // by recency.
  std::list<ui::ClipboardData> history_list_;

  // Factory to create WeakPtrs used to debounce calls to CommitData().
  base::WeakPtrFactory<ClipboardHistory> commit_data_weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_H_
