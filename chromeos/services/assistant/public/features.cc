// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/public/features.h"

#include "base/feature_list.h"

namespace chromeos {
namespace assistant {
namespace features {

const base::Feature kAssistantAudioEraser{"AssistantAudioEraser",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAssistantFeedbackUi{"AssistantFeedbackUi",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAssistantWarmerWelcomeFeature{
    "AssistantWarmerWelcome", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAssistantAppSupport{"AssistantAppSupport",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAssistantProactiveSuggestions{
    "AssistantProactiveSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

// The maximum width (in dip) for the proactive suggestions chip.
const base::FeatureParam<int> kAssistantProactiveSuggestionsMaxWidth{
    &kAssistantProactiveSuggestions, "max-width", 280};

const base::FeatureParam<std::string>
    kAssistantProactiveSuggestionsServerExperimentIds{
        &kAssistantProactiveSuggestions, "server-experiment-ids", ""};

const base::FeatureParam<bool> kAssistantProactiveSuggestionsSuppressDuplicates{
    &kAssistantProactiveSuggestions, "suppress-duplicates", true};

const base::FeatureParam<int>
    kAssistantProactiveSuggestionsTimeoutThresholdMillis{
        &kAssistantProactiveSuggestions, "timeout-threshold-millis", 15 * 1000};

const base::Feature kAssistantRoutines{"AssistantRoutines",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kInAssistantNotifications{
    "InAssistantNotifications", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableClearCutLog{"EnableClearCutLog",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableDspHotword{"EnableDspHotword",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableStereoAudioInput{"AssistantEnableStereoAudioInput",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTimerNotification{"ChromeOSAssistantTimerNotification",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEnableTextQueriesWithClientDiscourseContext{
    "AssistantEnableTextQueriesWithClientDiscourseContext",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTimerTicks{"ChromeOSAssistantTimerTicks",
                                base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEnableAssistantAlarmTimerManager{
    "EnableAssistantAlarmTimerManager", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEnablePowerManager{"ChromeOSAssistantEnablePowerManager",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables sending a screen context request ("What's on my screen?" and
// metalayer selection) as a text query. This is as opposed to sending
// the request as a contextual cards request.
const base::Feature kScreenContextQuery{"ChromeOSAssistantScreenContextQuery",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableMediaSessionIntegration{
    "AssistantEnableMediaSessionIntegration",
    base::FEATURE_DISABLED_BY_DEFAULT};

int GetProactiveSuggestionsMaxWidth() {
  return kAssistantProactiveSuggestionsMaxWidth.Get();
}

std::string GetProactiveSuggestionsServerExperimentIds() {
  return kAssistantProactiveSuggestionsServerExperimentIds.Get();
}

base::TimeDelta GetProactiveSuggestionsTimeoutThreshold() {
  return base::TimeDelta::FromMilliseconds(
      kAssistantProactiveSuggestionsTimeoutThresholdMillis.Get());
}

bool IsAlarmTimerManagerEnabled() {
  return base::FeatureList::IsEnabled(kEnableAssistantAlarmTimerManager);
}

bool IsAppSupportEnabled() {
  return base::FeatureList::IsEnabled(
      assistant::features::kAssistantAppSupport);
}

bool IsAudioEraserEnabled() {
  return base::FeatureList::IsEnabled(kAssistantAudioEraser);
}

bool IsClearCutLogEnabled() {
  return base::FeatureList::IsEnabled(kEnableClearCutLog);
}

bool IsDspHotwordEnabled() {
  return base::FeatureList::IsEnabled(kEnableDspHotword);
}

bool IsFeedbackUiEnabled() {
  return base::FeatureList::IsEnabled(kAssistantFeedbackUi);
}

bool IsInAssistantNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kInAssistantNotifications);
}

bool IsMediaSessionIntegrationEnabled() {
  return base::FeatureList::IsEnabled(kEnableMediaSessionIntegration);
}

bool IsPowerManagerEnabled() {
  return base::FeatureList::IsEnabled(kEnablePowerManager);
}

bool IsProactiveSuggestionsEnabled() {
  return base::FeatureList::IsEnabled(kAssistantProactiveSuggestions);
}

bool IsProactiveSuggestionsSuppressDuplicatesEnabled() {
  return kAssistantProactiveSuggestionsSuppressDuplicates.Get();
}

bool IsRoutinesEnabled() {
  return base::FeatureList::IsEnabled(kAssistantRoutines);
}

bool IsScreenContextQueryEnabled() {
  return base::FeatureList::IsEnabled(kScreenContextQuery);
}

bool IsStereoAudioInputEnabled() {
  return base::FeatureList::IsEnabled(kEnableStereoAudioInput) ||
         // Audio eraser requires 2 channel input.
         base::FeatureList::IsEnabled(kAssistantAudioEraser);
}

bool IsTimerNotificationEnabled() {
  return base::FeatureList::IsEnabled(kTimerNotification);
}

bool IsTimerTicksEnabled() {
  // The timer ticks feature is dependent on new notification add/remove logic
  // that is tied to new events delivered from the AlarmTimerManager API.
  return IsAlarmTimerManagerEnabled() &&
         base::FeatureList::IsEnabled(kTimerTicks);
}

bool IsWarmerWelcomeEnabled() {
  return base::FeatureList::IsEnabled(kAssistantWarmerWelcomeFeature);
}

}  // namespace features
}  // namespace assistant
}  // namespace chromeos
