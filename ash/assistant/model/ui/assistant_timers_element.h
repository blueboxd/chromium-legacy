// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_UI_ASSISTANT_TIMERS_ELEMENT_H_
#define ASH_ASSISTANT_MODEL_UI_ASSISTANT_TIMERS_ELEMENT_H_

#include <string>
#include <vector>

#include "ash/assistant/model/ui/assistant_ui_element.h"
#include "base/component_export.h"

namespace ash {

// An Assistant UI element that is bound to and renders a set of timers.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantTimersElement
    : public AssistantUiElement {
 public:
  explicit AssistantTimersElement(const std::vector<std::string>& timer_ids);
  AssistantTimersElement(const AssistantTimersElement&) = delete;
  AssistantTimersElement& operator=(const AssistantTimersElement&) = delete;
  ~AssistantTimersElement() override;

  const std::vector<std::string>& timer_ids() const { return timer_ids_; }

 private:
  std::vector<std::string> timer_ids_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_UI_ASSISTANT_TIMERS_ELEMENT_H_
