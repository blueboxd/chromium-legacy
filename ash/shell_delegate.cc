// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell_delegate.h"

namespace ash {

bool ShellDelegate::CreateBrowserForTabDrop(
    gfx::NativeWindow source_window,
    const ui::OSExchangeData& drop_data) {
  return false;
}

media_session::mojom::MediaSessionService*
ShellDelegate::GetMediaSessionService() {
  return nullptr;
}

}  // namespace ash
