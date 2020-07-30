// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history.h"

#include "ash/clipboard/clipboard_history_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/stl_util.h"
#include "components/account_id/account_id.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_non_backed.h"

namespace ash {

namespace {

constexpr size_t kMaxClipboardItemsShared = 5;

AccountId GetActiveAccountId() {
  return Shell::Get()->session_controller()->GetActiveAccountId();
}

}  // namespace

ClipboardHistory::ScopedPause::ScopedPause(ClipboardHistory* clipboard_history)
    : clipboard_history_(clipboard_history) {
  clipboard_history_->Pause();
}

ClipboardHistory::ScopedPause::~ScopedPause() {
  clipboard_history_->Resume();
}

ClipboardHistory::ClipboardHistory() {
  ui::ClipboardMonitor::GetInstance()->AddObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);
  Shell::Get()->AddShellObserver(this);
}

ClipboardHistory::~ClipboardHistory() {
  ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);
}

const std::list<ui::ClipboardData>& ClipboardHistory::GetItems() const {
  auto it = items_by_account_id_.find(GetActiveAccountId());
  DCHECK(it != items_by_account_id_.cend());
  return it->second;
}

void ClipboardHistory::Clear() {
  items_by_account_id_[GetActiveAccountId()] = std::list<ui::ClipboardData>();
}

bool ClipboardHistory::IsEmpty() const {
  return GetItems().empty();
}

void ClipboardHistory::OnClipboardDataChanged() {
  // TODO(newcomer): Prevent Clipboard from recording metrics when pausing
  // observation.
  if (num_pause_ > 0)
    return;

  auto* clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  CHECK(clipboard);

  const auto* clipboard_data = clipboard->GetClipboardData();
  CHECK(clipboard_data);

  // We post commit |clipboard_data| at the end of the current task sequence to
  // debounce the case where multiple copies are programmatically performed.
  // Since only the most recent copy will be at the top of the clipboard, the
  // user will likely be unaware of the intermediate copies that took place
  // opaquely in the same task sequence and would be confused to see them in
  // history. A real world example would be copying the URL from the address bar
  // in the browser. First a short form of the URL is copied, followed
  // immediately by the long form URL.
  commit_data_weak_factory_.InvalidateWeakPtrs();
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ClipboardHistory::CommitData,
                                commit_data_weak_factory_.GetWeakPtr(),
                                GetActiveAccountId(), *clipboard_data));
}

void ClipboardHistory::CommitData(const AccountId& account_id,
                                  ui::ClipboardData data) {
  std::list<ui::ClipboardData>& items = items_by_account_id_[account_id];

  // If |data| is already contained in |items|, at *most* we need to move it to
  // the front to retain sort order by recency.
  auto it = std::find(items.begin(), items.end(), data);
  if (it != items.end()) {
    if (it != items.begin())
      items.splice(items.begin(), items, it, std::next(it));
    return;
  }

  items.push_front(std::move(data));

  if (items.size() > kMaxClipboardItemsShared)
    items.pop_back();
}

void ClipboardHistory::Pause() {
  ++num_pause_;
}

void ClipboardHistory::Resume() {
  --num_pause_;
}

void ClipboardHistory::OnActiveUserSessionChanged(
    const AccountId& active_account_id) {
  if (!base::Contains(items_by_account_id_, active_account_id))
    items_by_account_id_[active_account_id] = std::list<ui::ClipboardData>();
}

void ClipboardHistory::OnShellDestroying() {
  // ClipboardHistory depends on Shell to access the classes it observes. So
  // remove itself from observer lists before the Shell instance is destroyed.
  Shell::Get()->session_controller()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
}

}  // namespace ash
