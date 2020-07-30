// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_H_

#include <list>
#include <map>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/shell_observer.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_observer.h"

class AccountId;

namespace ui {
class ClipboardData;
}  // namespace ui

namespace ash {

// Keeps track of the last few things saved in the clipboard. For multiprofile,
// one account's clipboard history is separated from others and indexed by the
// account id.
class ASH_EXPORT ClipboardHistory : public ui::ClipboardObserver,
                                    public SessionObserver,
                                    public ShellObserver {
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

  // Returns the list of most recent items copied by the active account. The
  // returned list is sorted by recency.
  const std::list<ui::ClipboardData>& GetItems() const;

  // Deletes clipboard history of the active account. Does not modify content
  // stored in the clipboard.
  void Clear();

  // Returns whether the clipboard history of the active account is empty.
  bool IsEmpty() const;

  // ClipboardMonitor:
  void OnClipboardDataChanged() override;

 private:
  // Adds |data| to the clipboard history associated with |account_id|.
  void CommitData(const AccountId& account_id, ui::ClipboardData data);
  void Pause();
  void Resume();

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // ShellObserver:
  void OnShellDestroying() override;

  // The count of pauses.
  size_t num_pause_ = 0;

  // Clipboard history is mapped by account ID to store different items per
  // account when multiprofile is used. Lists of items are sorted by recency.
  std::map<AccountId, std::list<ui::ClipboardData>> items_by_account_id_;

  // Factory to create WeakPtrs used to debounce calls to CommitData().
  base::WeakPtrFactory<ClipboardHistory> commit_data_weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_H_
