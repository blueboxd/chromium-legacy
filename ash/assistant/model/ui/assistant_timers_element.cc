// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/ui/assistant_timers_element.h"

namespace ash {

AssistantTimersElement::AssistantTimersElement(
    const std::vector<std::string>& timer_ids)
    : AssistantUiElement(AssistantUiElementType::kTimers),
      timer_ids_(timer_ids) {}

AssistantTimersElement::~AssistantTimersElement() = default;

}  // namespace ash
