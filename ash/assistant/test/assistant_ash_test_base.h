// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_TEST_ASSISTANT_ASH_TEST_BASE_H_
#define ASH_ASSISTANT_TEST_ASSISTANT_ASH_TEST_BASE_H_

#include <memory>

#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class AssistantController;
class AssistantMainView;
class AssistantPageView;
class ContentsView;

// Helper class to make testing the Assistant Ash UI easier.
class AssistantAshTestBase : public AshTestBase {
 public:
  AssistantAshTestBase();
  ~AssistantAshTestBase() override;

  void SetUp() override;
  void TearDown() override;

  // Return the actual displayed Assistant main view.
  // Can only be used after |ShowAssistantUi| has been called.
  const AssistantMainView* main_view() const;

  // This is the top-level Assistant specific view.
  // Can only be used after |ShowAssistantUi| has been called.
  const AssistantPageView* page_view() const;

  // Spoof sending a request to the Assistant service,
  // and receiving |response_text| as a response to display.
  void MockAssistantInteractionWithResponse(const std::string& response_text);

  void ShowAssistantUi(
      AssistantEntryPoint entry_point = AssistantEntryPoint::kUnspecified);

 private:
  const ContentsView* contents_view() const;

  void DisableAnimations();

  void ReenableAnimations();

  base::test::ScopedFeatureList scoped_feature_list_;
  AssistantController* controller_ = nullptr;
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode>
      scoped_animation_duration_;

  DISALLOW_COPY_AND_ASSIGN(AssistantAshTestBase);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_TEST_ASSISTANT_ASH_TEST_BASE_H_
