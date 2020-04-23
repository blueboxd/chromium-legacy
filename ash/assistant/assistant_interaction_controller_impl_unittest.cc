// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_interaction_controller_impl.h"

#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/test/fake_android_intent_helper.h"
#include "base/bind.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom-forward.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

namespace {

using chromeos::assistant::mojom::AndroidAppInfo;
using chromeos::assistant::mojom::AndroidAppInfoPtr;
using chromeos::assistant::mojom::AssistantInteractionMetadata;
using chromeos::assistant::mojom::AssistantInteractionType;
using ::testing::StrictMock;

constexpr bool kErrorResult = false;
constexpr bool kSuccessResult = true;

// A simple mock to allow mocking of the callback passed to |OnOpenAppResponse|.
// Example usage:
//
//    OpenAppCallbackMock callback;
//
//    EXPECT_CALL(callback, Callback(true));
//    FunctionTakingACallback(callback.Bind());
class OpenAppCallbackMock {
 public:
  MOCK_METHOD(void, Callback, (bool));

  auto Bind() {
    return base::BindOnce(&OpenAppCallbackMock::Callback,
                          base::Unretained(this));
  }
};

class AssistantInteractionControllerImplTest : public AssistantAshTestBase {
 public:
  AssistantInteractionControllerImplTest() = default;

  AssistantInteractionControllerImpl* interaction_controller() {
    return static_cast<AssistantInteractionControllerImpl*>(
        AssistantInteractionController::Get());
  }

  const AssistantInteractionModel* interaction_model() {
    return interaction_controller()->GetModel();
  }

  void StartInteraction() {
    interaction_controller()->OnInteractionStarted(
        AssistantInteractionMetadata::New());
  }

  AndroidAppInfoPtr CreateAndroidAppInfo(
      const std::string& app_name = "unknown") {
    AndroidAppInfoPtr result = AndroidAppInfo::New();
    result->localized_app_name = app_name;
    return result;
  }
};

}  // namespace

TEST_F(AssistantInteractionControllerImplTest,
       ShouldBecomeActiveWhenInteractionStarts) {
  EXPECT_EQ(interaction_model()->interaction_state(),
            InteractionState::kInactive);

  interaction_controller()->OnInteractionStarted(
      AssistantInteractionMetadata::New());

  EXPECT_EQ(interaction_model()->interaction_state(),
            InteractionState::kActive);
}

TEST_F(AssistantInteractionControllerImplTest,
       ShouldCallCallbackWhenOpenAppIsCalledWhileInactive) {
  StrictMock<OpenAppCallbackMock> callback;

  EXPECT_CALL(callback, Callback(kErrorResult));
  EXPECT_EQ(interaction_model()->interaction_state(),
            InteractionState::kInactive);

  interaction_controller()->OnOpenAppResponse(CreateAndroidAppInfo(),
                                              callback.Bind());
}

TEST_F(AssistantInteractionControllerImplTest,
       ShouldCallCallbackWhenOpenAppIsCalledWithoutAnAndroidIntentHelper) {
  StrictMock<OpenAppCallbackMock> callback;

  StartInteraction();
  EXPECT_CALL(callback, Callback(kErrorResult));

  interaction_controller()->OnOpenAppResponse(CreateAndroidAppInfo(),
                                              callback.Bind());
}

TEST_F(AssistantInteractionControllerImplTest,
       ShouldCallCallbackWhenOpenAppIsCalledForUnknownAndroidApp) {
  StrictMock<OpenAppCallbackMock> callback;

  StartInteraction();
  FakeAndroidIntentHelper fake_helper;
  EXPECT_CALL(callback, Callback(kErrorResult));

  interaction_controller()->OnOpenAppResponse(
      CreateAndroidAppInfo("unknown-app-name"), callback.Bind());
}

TEST_F(AssistantInteractionControllerImplTest,
       ShouldLaunchAppAndCallCallbackWhenOpenAppIsCalled) {
  StrictMock<OpenAppCallbackMock> callback;
  const std::string app_name = "AppName";
  const std::string intent = "intent://AppName";

  StartInteraction();
  FakeAndroidIntentHelper fake_helper;
  fake_helper.AddApp(app_name, intent);
  EXPECT_CALL(callback, Callback(kSuccessResult));

  interaction_controller()->OnOpenAppResponse(CreateAndroidAppInfo(app_name),
                                              callback.Bind());

  EXPECT_EQ(intent, fake_helper.last_launched_android_intent());
}

TEST_F(AssistantInteractionControllerImplTest,
       ShouldAddSchemeToIntentWhenLaunchingAndroidApp) {
  StrictMock<OpenAppCallbackMock> callback;
  const std::string app_name = "AppName";
  const std::string intent = "#Intent-without-a-scheme";
  const std::string intent_with_scheme = "intent://" + intent;

  StartInteraction();
  FakeAndroidIntentHelper fake_helper;
  fake_helper.AddApp(app_name, intent);
  EXPECT_CALL(callback, Callback(kSuccessResult));

  interaction_controller()->OnOpenAppResponse(CreateAndroidAppInfo(app_name),
                                              callback.Bind());

  EXPECT_EQ(intent_with_scheme, fake_helper.last_launched_android_intent());
}

}  // namespace ash
