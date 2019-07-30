// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/assistant_state_base.h"

#include <ostream>
#include <sstream>

#include "ash/public/cpp/accelerators.h"
#include "base/strings/string_piece_forward.h"

namespace ash {
namespace {
template <typename T>
void PrintValue(std::stringstream* result,
                const std::string& name,
                const base::Optional<T>& value) {
  *result << std::endl << "  " << name << ": ";
  if (value.has_value())
    *result << value.value();
  else
    *result << ("(no value)");
}

#define PRINT_VALUE(value) PrintValue(&result, #value, value())
}  // namespace

AssistantStateBase::AssistantStateBase() = default;

AssistantStateBase::~AssistantStateBase() = default;

std::string AssistantStateBase::ToString() const {
  std::stringstream result;
  result << "AssistantState:";
  PRINT_VALUE(voice_interaction_state);
  PRINT_VALUE(settings_enabled);
  PRINT_VALUE(context_enabled);
  PRINT_VALUE(hotword_enabled);
  PRINT_VALUE(allowed_state);
  PRINT_VALUE(locale);
  PRINT_VALUE(arc_play_store_enabled);
  PRINT_VALUE(locked_full_screen_enabled);
  return result.str();
}

}  // namespace ash
