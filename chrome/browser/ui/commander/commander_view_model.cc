// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/commander_view_model.h"

namespace commander {

CommandItemViewModel::CommandItemViewModel(
    base::string16& title,
    std::vector<gfx::Range>& matched_ranges)
    : title(title), matched_ranges(matched_ranges) {}
CommandItemViewModel::~CommandItemViewModel() = default;
CommandItemViewModel::CommandItemViewModel(const CommandItemViewModel& other) =
    default;

CommanderViewModel::CommanderViewModel() = default;
CommanderViewModel::~CommanderViewModel() = default;
CommanderViewModel::CommanderViewModel(const CommanderViewModel& other) =
    default;

}  // namespace commander
