// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/public/cpp/features.h"

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

const base::Feature kAssistantConversationStartersV2{
    "AssistantConversationStartersV2", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAssistantLauncherChipIntegration{
    "AssistantLauncherChipIntegration", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAssistantProactiveSuggestions{
    "AssistantProactiveSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

// The maximum width (in dip) for the proactive suggestions chip.
const base::FeatureParam<int> kAssistantProactiveSuggestionsMaxWidth{
    &kAssistantProactiveSuggestions, "max-width", 280};

// The desired background blur radius (in dip) for the rich proactive
// suggestions entry point. Amount of blur may need to be dynamically modified
// later or disabled which may be accomplished by setting to zero.
const base::FeatureParam<int>
    kAssistantProactiveSuggestionsRichEntryPointBackgroundBlurRadius{
        &kAssistantProactiveSuggestions,
        "rich-entry-point-background-blur-radius", 30};

// The desired corner radius (in dip) for the rich proactive suggestions entry
// point. As the rich UI has yet to be defined, corner radius may need to be
// dynamically modified later.
const base::FeatureParam<int>
    kAssistantProactiveSuggestionsRichEntryPointCornerRadius{
        &kAssistantProactiveSuggestions, "rich-entry-point-corner-radius", 12};

const base::FeatureParam<std::string>
    kAssistantProactiveSuggestionsServerExperimentIds{
        &kAssistantProactiveSuggestions, "server-experiment-ids", ""};

// When enabled, the proactive suggestions view will show only after the user
// scrolls up in the source web contents. When disabled, the view will be shown
// immediately once the set of proactive suggestions are available.
const base::FeatureParam<bool> kAssistantProactiveSuggestionsShowOnScroll{
    &kAssistantProactiveSuggestions, "show-on-scroll", true};

// When enabled, we will use the rich, content-forward entry point for the
// proactive suggestions feature in lieu of the simple entry point affordance.
const base::FeatureParam<bool> kAssistantProactiveSuggestionsShowRichEntryPoint{
    &kAssistantProactiveSuggestions, "show-rich-entry-point", false};

const base::FeatureParam<bool> kAssistantProactiveSuggestionsSuppressDuplicates{
    &kAssistantProactiveSuggestions, "suppress-duplicates", false};

const base::FeatureParam<int>
    kAssistantProactiveSuggestionsTimeoutThresholdMillis{
        &kAssistantProactiveSuggestions, "timeout-threshold-millis", 15 * 1000};

// When enabled, Assistant will use response processing V2. This is a set of
// synced client and server changes which will turn on default parallel client
// op processing and eager (streaming) UI element rendering.
const base::Feature kAssistantResponseProcessingV2{
    "AssistantResponseProcessingV2", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAssistantRoutines{"AssistantRoutines",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAssistantTimersV2{"AssistantTimersV2",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableClearCutLog{"EnableClearCutLog",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableDspHotword{"EnableDspHotword",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableStereoAudioInput{"AssistantEnableStereoAudioInput",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnablePowerManager{"ChromeOSAssistantEnablePowerManager",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableOnDeviceAssistant{"ChromeOSOnDeviceAssistant",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableMediaSessionIntegration{
    "AssistantEnableMediaSessionIntegration",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Disable voice match for test purpose.
const base::Feature kDisableVoiceMatch{"DisableVoiceMatch",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

int GetProactiveSuggestionsMaxWidth() {
  return kAssistantProactiveSuggestionsMaxWidth.Get();
}

int GetProactiveSuggestionsRichEntryPointBackgroundBlurRadius() {
  return kAssistantProactiveSuggestionsRichEntryPointBackgroundBlurRadius.Get();
}

int GetProactiveSuggestionsRichEntryPointCornerRadius() {
  return kAssistantProactiveSuggestionsRichEntryPointCornerRadius.Get();
}

std::string GetProactiveSuggestionsServerExperimentIds() {
  return kAssistantProactiveSuggestionsServerExperimentIds.Get();
}

base::TimeDelta GetProactiveSuggestionsTimeoutThreshold() {
  return base::TimeDelta::FromMilliseconds(
      kAssistantProactiveSuggestionsTimeoutThresholdMillis.Get());
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

bool IsConversationStartersV2Enabled() {
  return base::FeatureList::IsEnabled(kAssistantConversationStartersV2);
}

bool IsDspHotwordEnabled() {
  return base::FeatureList::IsEnabled(kEnableDspHotword);
}

bool IsFeedbackUiEnabled() {
  return base::FeatureList::IsEnabled(kAssistantFeedbackUi);
}

bool IsLauncherChipIntegrationEnabled() {
  return base::FeatureList::IsEnabled(kAssistantLauncherChipIntegration);
}

bool IsMediaSessionIntegrationEnabled() {
  return base::FeatureList::IsEnabled(kEnableMediaSessionIntegration);
}

bool IsPowerManagerEnabled() {
  return base::FeatureList::IsEnabled(kEnablePowerManager);
}

bool IsOnDeviceAssistantEnabled() {
  return base::FeatureList::IsEnabled(kEnableOnDeviceAssistant);
}

bool IsProactiveSuggestionsEnabled() {
  return base::FeatureList::IsEnabled(kAssistantProactiveSuggestions);
}

bool IsProactiveSuggestionsShowOnScrollEnabled() {
  return kAssistantProactiveSuggestionsShowOnScroll.Get();
}

bool IsProactiveSuggestionsShowRichEntryPointEnabled() {
  return kAssistantProactiveSuggestionsShowRichEntryPoint.Get();
}

bool IsProactiveSuggestionsSuppressDuplicatesEnabled() {
  return kAssistantProactiveSuggestionsSuppressDuplicates.Get();
}

bool IsResponseProcessingV2Enabled() {
  return base::FeatureList::IsEnabled(kAssistantResponseProcessingV2);
}

bool IsRoutinesEnabled() {
  return base::FeatureList::IsEnabled(kAssistantRoutines);
}

bool IsStereoAudioInputEnabled() {
  return base::FeatureList::IsEnabled(kEnableStereoAudioInput) ||
         // Audio eraser requires 2 channel input.
         base::FeatureList::IsEnabled(kAssistantAudioEraser);
}

bool IsTimersV2Enabled() {
  return base::FeatureList::IsEnabled(kAssistantTimersV2);
}

bool IsWarmerWelcomeEnabled() {
  return base::FeatureList::IsEnabled(kAssistantWarmerWelcomeFeature);
}

bool IsVoiceMatchDisabled() {
  return base::FeatureList::IsEnabled(kDisableVoiceMatch);
}

}  // namespace features
}  // namespace assistant
}  // namespace chromeos
