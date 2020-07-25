// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_H_

#include <deque>
#include <map>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/shell_observer.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_observer.h"

class AccountId;

namespace ui {
class ClipboardData;
}

namespace ash {

// Keeps track of the last few things saved in the clipboard. For multiprofile,
// one account's clipboard history is separated from others and indexed by the
// account id.
class ASH_EXPORT ClipboardHistory : public ui::ClipboardObserver,
                                    public SessionObserver,
                                    public ShellObserver {
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

  // Deletes clipboard history of the active account. Does not modify content
  // stored in the clipboard.
  void ClearHistory();

  // Returns the recent unique clipboard data of the active account.
  std::vector<ui::ClipboardData> GetRecentClipboardDataWithNoDuplicates() const;

  // Returns whether the clipboard history of the active account is empty.
  bool IsEmpty() const;

  // ClipboardMonitor:
  void OnClipboardDataChanged() override;

 private:
  // Adds |data| to the clipboard history belonging to the account indicated
  // by |account_id|.
  void CommitData(const AccountId& account_id, ui::ClipboardData data);

  // Callback to read a bitmap from the clipboard data belonging to the account
  // indicated by |active_account_id|.
  void OnRecievePNGFromClipboard(const AccountId& active_account_id,
                                 ui::ClipboardData data,
                                 const SkBitmap& bitmap);
  void PauseClipboardHistory();
  void UnPauseClipboardHistory();

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // ShellObserver:
  void OnShellDestroying() override;

  // The count of clipboard history pauses.
  size_t num_pause_clipboard_history_ = 0;

  // Clipboard history is mapped by account ID to store different histories per
  // account when multiprofile is used. Duplicates are kept to maintain the most
  // recent ordering.
  std::map<AccountId, std::deque<ui::ClipboardData>>
      history_with_duplicates_mappings_;

  base::WeakPtrFactory<ClipboardHistory> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_H_
