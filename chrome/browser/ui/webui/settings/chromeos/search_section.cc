// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/search_section.h"

#include "ash/public/cpp/assistant/assistant_state.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/assistant/assistant_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_utils.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetSearchPageSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Search" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAssistantSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Assistant" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAssistantOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Assistant on" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAssistantOffSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Assistant off" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAssistantQuickAnswersSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Assistant Quick Answers" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAssistantHotwordDspSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Assistant hotword DSP" search concepts.
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAssistantVoiceMatchSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      // TODO(khorimoto): Add "Assistant hotword DSP" search concepts.
  });
  return *tags;
}

bool IsVoiceMatchAllowed() {
  return !assistant::features::IsVoiceMatchDisabled();
}

bool AreQuickAnswersAllowed() {
  return features::IsQuickAnswersSettingToggleEnabled();
}

void AddGoogleAssistantStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"googleAssistantPageTitle", IDS_SETTINGS_GOOGLE_ASSISTANT},
      {"googleAssistantEnableContext", IDS_ASSISTANT_SCREEN_CONTEXT_TITLE},
      {"googleAssistantEnableContextDescription",
       IDS_ASSISTANT_SCREEN_CONTEXT_DESC},
      {"googleAssistantEnableQuickAnswers",
       IDS_ASSISTANT_QUICK_ANSWERS_SETTING_TITLE},
      {"googleAssistantEnableQuickAnswersDescription",
       IDS_ASSISTANT_QUICK_ANSWERS_SETTING_DESC},
      {"googleAssistantEnableHotword",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_HOTWORD},
      {"googleAssistantEnableHotwordDescription",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_HOTWORD_DESCRIPTION},
      {"googleAssistantVoiceSettings",
       IDS_SETTINGS_GOOGLE_ASSISTANT_VOICE_SETTINGS},
      {"googleAssistantVoiceSettingsDescription",
       IDS_ASSISTANT_VOICE_MATCH_RECORDING},
      {"googleAssistantVoiceSettingsRetrainButton",
       IDS_SETTINGS_GOOGLE_ASSISTANT_VOICE_SETTINGS_RETRAIN},
      {"googleAssistantEnableHotwordWithoutDspDescription",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_HOTWORD_WITHOUT_DSP_DESCRIPTION},
      {"googleAssistantEnableHotwordWithoutDspRecommended",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_HOTWORD_WITHOUT_DSP_RECOMMENDED},
      {"googleAssistantEnableHotwordWithoutDspAlwaysOn",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_HOTWORD_WITHOUT_DSP_ALWAYS_ON},
      {"googleAssistantEnableHotwordWithoutDspOff",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_HOTWORD_WITHOUT_DSP_OFF},
      {"googleAssistantEnableNotification",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_NOTIFICATION},
      {"googleAssistantEnableNotificationDescription",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_NOTIFICATION_DESCRIPTION},
      {"googleAssistantLaunchWithMicOpen",
       IDS_SETTINGS_GOOGLE_ASSISTANT_LAUNCH_WITH_MIC_OPEN},
      {"googleAssistantLaunchWithMicOpenDescription",
       IDS_SETTINGS_GOOGLE_ASSISTANT_LAUNCH_WITH_MIC_OPEN_DESCRIPTION},
      {"googleAssistantSettings", IDS_SETTINGS_GOOGLE_ASSISTANT_SETTINGS},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddBoolean("hotwordDspAvailable", IsHotwordDspAvailable());
  html_source->AddBoolean("voiceMatchDisabled", !IsVoiceMatchAllowed());
  html_source->AddBoolean("quickAnswersAvailable", AreQuickAnswersAllowed());
}

}  // namespace

SearchSection::SearchSection(Profile* profile, Delegate* per_page_delegate)
    : OsSettingsSection(profile, per_page_delegate) {
  delegate()->AddSearchTags(GetSearchPageSearchConcepts());

  ash::AssistantState* assistant_state = ash::AssistantState::Get();
  if (IsAssistantAllowed() && assistant_state) {
    delegate()->AddSearchTags(GetAssistantSearchConcepts());

    assistant_state->AddObserver(this);
    UpdateAssistantSearchTags();
  }
}

SearchSection::~SearchSection() {
  ash::AssistantState* assistant_state = ash::AssistantState::Get();
  if (IsAssistantAllowed() && assistant_state)
    assistant_state->RemoveObserver(this);
}

void SearchSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"osSearchEngineLabel", IDS_OS_SETTINGS_SEARCH_ENGINE_LABEL},
      {"searchGoogleAssistant", IDS_SETTINGS_SEARCH_GOOGLE_ASSISTANT},
      {"searchGoogleAssistantEnabled",
       IDS_SETTINGS_SEARCH_GOOGLE_ASSISTANT_ENABLED},
      {"searchGoogleAssistantDisabled",
       IDS_SETTINGS_SEARCH_GOOGLE_ASSISTANT_DISABLED},
      {"searchGoogleAssistantOn", IDS_SETTINGS_SEARCH_GOOGLE_ASSISTANT_ON},
      {"searchGoogleAssistantOff", IDS_SETTINGS_SEARCH_GOOGLE_ASSISTANT_OFF},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  const bool is_assistant_allowed = IsAssistantAllowed();
  html_source->AddBoolean("isAssistantAllowed", is_assistant_allowed);
  html_source->AddLocalizedString("osSearchPageTitle",
                                  is_assistant_allowed
                                      ? IDS_SETTINGS_SEARCH_AND_ASSISTANT
                                      : IDS_SETTINGS_SEARCH);
  html_source->AddString("searchExplanation",
                         l10n_util::GetStringFUTF16(
                             IDS_SETTINGS_SEARCH_EXPLANATION,
                             base::ASCIIToUTF16(chrome::kOmniboxLearnMoreURL)));
  html_source->AddString(
      "osSearchEngineTooltip",
      ui::SubstituteChromeOSDeviceType(IDS_OS_SETTINGS_SEARCH_ENGINE_TOOLTIP));

  AddGoogleAssistantStrings(html_source);
}

void SearchSection::OnAssistantConsentStatusChanged(int consent_status) {
  UpdateAssistantSearchTags();
}

void SearchSection::OnAssistantContextEnabled(bool enabled) {
  UpdateAssistantSearchTags();
}

void SearchSection::OnAssistantSettingsEnabled(bool enabled) {
  UpdateAssistantSearchTags();
}

void SearchSection::OnAssistantHotwordEnabled(bool enabled) {
  UpdateAssistantSearchTags();
}

bool SearchSection::IsAssistantAllowed() {
  // NOTE: This will be false when the flag is disabled.
  return ::assistant::IsAssistantAllowedForProfile(profile()) ==
         ash::mojom::AssistantAllowedState::ALLOWED;
}

void SearchSection::UpdateAssistantSearchTags() {
  // Start without any Assistant search concepts, then add if needed below.
  delegate()->RemoveSearchTags(GetAssistantOnSearchConcepts());
  delegate()->RemoveSearchTags(GetAssistantOffSearchConcepts());
  delegate()->RemoveSearchTags(GetAssistantQuickAnswersSearchConcepts());
  delegate()->RemoveSearchTags(GetAssistantHotwordDspSearchConcepts());
  delegate()->RemoveSearchTags(GetAssistantVoiceMatchSearchConcepts());

  ash::AssistantState* assistant_state = ash::AssistantState::Get();

  // The setting_enabled() function is the top-level enabled state. If this is
  // off, none of the sub-features are enabled.
  if (!assistant_state->settings_enabled() ||
      !assistant_state->settings_enabled().value()) {
    delegate()->AddSearchTags(GetAssistantOffSearchConcepts());
    return;
  }

  delegate()->AddSearchTags(GetAssistantOnSearchConcepts());

  if (AreQuickAnswersAllowed() && assistant_state->context_enabled() &&
      assistant_state->context_enabled().value()) {
    delegate()->AddSearchTags(GetAssistantQuickAnswersSearchConcepts());
  }

  if (IsHotwordDspAvailable())
    delegate()->AddSearchTags(GetAssistantHotwordDspSearchConcepts());

  if (IsVoiceMatchAllowed() && assistant_state->hotword_enabled() &&
      assistant_state->hotword_enabled().value() &&
      assistant_state->consent_status() &&
      assistant_state->consent_status().value() ==
          assistant::prefs::ConsentStatus::kActivityControlAccepted) {
    delegate()->AddSearchTags(GetAssistantVoiceMatchSearchConcepts());
  }
}

}  // namespace settings
}  // namespace chromeos
