// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_TEST_API_IMPL_H_
#define ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_TEST_API_IMPL_H_

#include <memory>
#include <string>

#include "ash/public/cpp/test/assistant_test_api.h"
#include "base/macros.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ui {
class ScopedAnimationDurationScaleMode;
}  // namespace ui

namespace ash {

class ContentsView;

class AssistantTestApiImpl : public AssistantTestApi {
 public:
  AssistantTestApiImpl();
  ~AssistantTestApiImpl() override;

  // AssistantTestApi:
  void DisableAnimations() override;

  bool IsVisible() override;

  void SendTextQuery(const std::string& query) override;

  void EnableAssistant() override;
  void SetTabletMode(bool enable) override;
  void SetPreferVoice(bool value) override;

  views::View* page_view() override;
  views::View* main_view() override;
  views::Textfield* input_text_field() override;
  views::View* mic_view() override;
  views::View* greeting_label() override;
  views::View* voice_input_toggle() override;
  views::View* keyboard_input_toggle() override;
  aura::Window* window() override;
  views::View* app_list_view() override;

 private:
  void EnableAnimations();

  ContentsView* contents_view();

  void SendKeyPress(ui::KeyboardCode key);

  std::unique_ptr<ui::ScopedAnimationDurationScaleMode>
      scoped_animation_duration_;

  DISALLOW_COPY_AND_ASSIGN(AssistantTestApiImpl);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_TEST_API_IMPL_H_
