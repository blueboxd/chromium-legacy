// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/quick_answers_client.h"

#include <utility>

#include "base/strings/stringprintf.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/re2/src/re2/re2.h"

namespace chromeos {
namespace quick_answers {
namespace {

using network::mojom::URLLoaderFactory;

constexpr char kAddressRegex[] =
    "^\\d+\\s[A-z]+\\s[A-z]+, ([A-z]|\\s)+, [A-z]{2}\\s[0-9]{5}";
constexpr char kDirectionQueryRewriteTemplate[] = "Direction to %s";

const QuickAnswersRequest PreprocessRequest(
    const QuickAnswersRequest& request) {
  QuickAnswersRequest processed_request = request;
  // Temporarily classify text for demo purpose only. This will be replaced with
  // TCLib when it is ready.
  // TODO(llin): Query TCLib and rewrite the query based on TCLib result.
  if (re2::RE2::FullMatch(request.selected_text, kAddressRegex)) {
    // TODO(llin): Add localization string for query rewrite.
    processed_request.selected_text =
        base::StringPrintf(kDirectionQueryRewriteTemplate,
                           processed_request.selected_text.c_str());
  }

  return processed_request;
}

}  // namespace

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
    ash::mojom::AssistantAllowedState state) {
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

void QuickAnswersClient::OnLocaleChanged(const std::string& locale) {
  const std::string kAllowedLocales[] = {ULOC_US};
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
  // Preprocess the request.
  auto& processed_request = PreprocessRequest(quick_answers_request);
  delegate_->OnRequestPreprocessFinish(processed_request);

  // Load and parse search result.
  search_results_loader_ = std::make_unique<SearchResultLoader>(
      url_loader_factory_,
      base::BindOnce(&QuickAnswersClient::OnQuickAnswerReceived,
                     base::Unretained(this)));
  search_results_loader_->Fetch(processed_request.selected_text);
}

void QuickAnswersClient::NotifyEligibilityChanged() {
  DCHECK(delegate_);
  bool is_eligible =
      (chromeos::features::IsQuickAnswersEnabled() && assistant_state_ &&
       assistant_enabled_ && locale_supported_ && assistant_context_enabled_ &&
       assistant_allowed_state_ == ash::mojom::AssistantAllowedState::ALLOWED);

  if (is_eligible_ != is_eligible) {
    is_eligible_ = is_eligible;
    delegate_->OnEligibilityChanged(is_eligible);
  }
}

void QuickAnswersClient::OnQuickAnswerReceived(
    std::unique_ptr<QuickAnswer> quick_answer) {
  DCHECK(delegate_);
  delegate_->OnQuickAnswerReceived(std::move(quick_answer));
}

}  // namespace quick_answers
}  // namespace chromeos
