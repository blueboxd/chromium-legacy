// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/quick_answers_client.h"

#include <utility>

#include "base/strings/stringprintf.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/utils/quick_answers_metrics.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace chromeos {
namespace quick_answers {
namespace {

using network::mojom::URLLoaderFactory;

constexpr char kDictionaryQueryRewriteTemplate[] = "Define:%s";
constexpr char kTranslationQueryRewriteTemplate[] = "Translate:%s";

QuickAnswersClient::ResultLoaderFactoryCallback*
    g_testing_result_factory_callback = nullptr;

QuickAnswersClient::IntentGeneratorFactoryCallback*
    g_testing_intent_generator_factory_callback = nullptr;

const PreprocessedOutput PreprocessRequest(const QuickAnswersRequest& request,
                                           const std::string& intent_text,
                                           IntentType intent_type) {
  PreprocessedOutput processed_output;
  processed_output.intent_text = intent_text;
  processed_output.query = intent_text;
  processed_output.intent_type = intent_type;

  switch (intent_type) {
    case IntentType::kUnit:
      break;
    case IntentType::kDictionary:
      processed_output.query = base::StringPrintf(
          kDictionaryQueryRewriteTemplate, intent_text.c_str());
      break;
    case IntentType::kTranslation:
      processed_output.query = base::StringPrintf(
          kTranslationQueryRewriteTemplate, intent_text.c_str());
      break;
    case IntentType::kUnknown:
      // TODO(llin): Update to NOTREACHED after integrating with TCLib.
      break;
  }
  return processed_output;
}

}  // namespace

// static
void QuickAnswersClient::SetResultLoaderFactoryForTesting(
    ResultLoaderFactoryCallback* factory) {
  g_testing_result_factory_callback = factory;
}

void QuickAnswersClient::SetIntentGeneratorFactoryForTesting(
    IntentGeneratorFactoryCallback* factory) {
  g_testing_intent_generator_factory_callback = factory;
}

QuickAnswersClient::QuickAnswersClient(URLLoaderFactory* url_loader_factory,
                                       ash::AssistantState* assistant_state,
                                       QuickAnswersDelegate* delegate)
    : url_loader_factory_(url_loader_factory),
      assistant_state_(assistant_state),
      delegate_(delegate) {
  if (assistant_state_) {
    // We observe Assistant state to detect enabling/disabling of Assistant in
    // settings as well as enabling/disabling of screen context.
    assistant_state_->AddObserver(this);
  }
}

QuickAnswersClient::~QuickAnswersClient() {
  if (assistant_state_)
    assistant_state_->RemoveObserver(this);
}

void QuickAnswersClient::OnAssistantFeatureAllowedChanged(
    chromeos::assistant::AssistantAllowedState state) {
  assistant_allowed_state_ = state;
  NotifyEligibilityChanged();
}

void QuickAnswersClient::OnAssistantSettingsEnabled(bool enabled) {
  assistant_enabled_ = enabled;
  NotifyEligibilityChanged();
}

void QuickAnswersClient::OnAssistantContextEnabled(bool enabled) {
  assistant_context_enabled_ = enabled;
  NotifyEligibilityChanged();
}

void QuickAnswersClient::OnAssistantQuickAnswersEnabled(bool enabled) {
  quick_answers_settings_enabled_ = enabled;
  NotifyEligibilityChanged();
}

void QuickAnswersClient::OnLocaleChanged(const std::string& locale) {
  // String literals used in some cases in the array because their
  // constant equivalents don't exist in:
  // third_party/icu/source/common/unicode/uloc.h
  const std::string kAllowedLocales[] = {ULOC_CANADA, ULOC_UK, ULOC_US,
                                         "en_AU",     "en_IN", "en_NZ"};

  const std::string kRuntimeLocale = icu::Locale::getDefault().getName();
  locale_supported_ = (base::Contains(kAllowedLocales, locale) ||
                       base::Contains(kAllowedLocales, kRuntimeLocale));
  NotifyEligibilityChanged();
}

void QuickAnswersClient::OnAssistantStateDestroyed() {
  assistant_state_ = nullptr;
}

void QuickAnswersClient::SendRequest(
    const QuickAnswersRequest& quick_answers_request) {
  RecordSelectedTextLength(quick_answers_request.selected_text.length());

  // Generate intent from |quick_answers_request|.
  intent_generator_ = CreateIntentGenerator(quick_answers_request);
  intent_generator_->GenerateIntent(quick_answers_request);
}

void QuickAnswersClient::OnQuickAnswerClick(ResultType result_type) {
  RecordClick(result_type, GetImpressionDuration());
}

void QuickAnswersClient::OnQuickAnswersDismissed(ResultType result_type,
                                                 bool is_active) {
  if (is_active)
    RecordActiveImpression(result_type, GetImpressionDuration());
}

void QuickAnswersClient::NotifyEligibilityChanged() {
  DCHECK(delegate_);

  bool is_eligible =
      (chromeos::features::IsQuickAnswersEnabled() && assistant_state_ &&
       assistant_enabled_ && locale_supported_ && assistant_context_enabled_ &&
       quick_answers_settings_enabled_ &&
       assistant_allowed_state_ ==
           chromeos::assistant::AssistantAllowedState::ALLOWED);

  if (is_eligible_ != is_eligible) {
    is_eligible_ = is_eligible;
    delegate_->OnEligibilityChanged(is_eligible);
  }
}

std::unique_ptr<ResultLoader> QuickAnswersClient::CreateResultLoader(
    IntentType intent_type) {
  if (g_testing_result_factory_callback)
    return g_testing_result_factory_callback->Run();
  return ResultLoader::Create(intent_type, url_loader_factory_, this);
}

std::unique_ptr<IntentGenerator> QuickAnswersClient::CreateIntentGenerator(
    const QuickAnswersRequest& request) {
  if (g_testing_intent_generator_factory_callback)
    return g_testing_intent_generator_factory_callback->Run();
  return std::make_unique<IntentGenerator>(
      base::BindOnce(&QuickAnswersClient::IntentGeneratorCallback,
                     weak_factory_.GetWeakPtr(), request));
}

void QuickAnswersClient::OnNetworkError() {
  DCHECK(delegate_);
  delegate_->OnNetworkError();
}

void QuickAnswersClient::OnQuickAnswerReceived(
    std::unique_ptr<QuickAnswer> quick_answer) {
  DCHECK(delegate_);
  quick_answer_received_time_ = base::TimeTicks::Now();
  delegate_->OnQuickAnswerReceived(std::move(quick_answer));
}

void QuickAnswersClient::IntentGeneratorCallback(
    const QuickAnswersRequest& quick_answers_request,
    const std::string& intent_text,
    IntentType intent_type) {
  DCHECK(delegate_);

  // Preprocess the request.
  QuickAnswersRequest processed_request = quick_answers_request;
  processed_request.preprocessed_output =
      PreprocessRequest(quick_answers_request, intent_text, intent_type);

  delegate_->OnRequestPreprocessFinish(processed_request);

  if (features::IsQuickAnswersTextAnnotatorEnabled() &&
      processed_request.preprocessed_output.intent_type ==
          IntentType::kUnknown) {
    // Don't fetch answer if no intent is generated.
    return;
  }

  result_loader_ = CreateResultLoader(intent_type);
  // Load and parse search result.
  result_loader_->Fetch(processed_request.preprocessed_output.query);
}

base::TimeDelta QuickAnswersClient::GetImpressionDuration() const {
  // Use default 0 duration.
  base::TimeDelta duration;
  if (!quick_answer_received_time_.is_null()) {
    // Fetch finish, set the duration to be between fetch finish and now.
    duration = base::TimeTicks::Now() - quick_answer_received_time_;
  }
  return duration;
}

}  // namespace quick_answers
}  // namespace chromeos
