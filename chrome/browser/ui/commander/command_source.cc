// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/command_source.h"

namespace commander {

CommandItem::CommandItem() = default;
CommandItem::~CommandItem() = default;
CommandItem::CommandItem(CommandItem&& other) = default;
CommandItem& CommandItem::operator=(CommandItem&& other) = default;

CommandItem::Type CommandItem::GetType() {
  DCHECK(!command || !delegate_factory);
  if (delegate_factory)
    return kComposite;
  return kOneShot;
}

}  // namespace commander
