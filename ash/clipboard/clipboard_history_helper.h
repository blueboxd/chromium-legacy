// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_HELPER_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_HELPER_H_

#include "ash/ash_export.h"
#include "base/strings/string16.h"

namespace ui {
class ClipboardData;
}  // namespace ui

namespace ash {
namespace clipboard {
namespace helper {

// Returns the label to display for the specified clipboard |data|.
ASH_EXPORT base::string16 GetLabel(const ui::ClipboardData& data);

}  // namespace helper
}  // namespace clipboard
}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_HELPER_H_
