// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/supervision/onboarding_flow_model.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/supervision/onboarding_constants.h"
#include "chrome/browser/chromeos/supervision/onboarding_delegate.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/constants/chromeos_switches.h"
#include "services/identity/public/cpp/primary_account_access_token_fetcher.h"
#include "url/gurl.h"

namespace chromeos {
namespace supervision {
namespace {

// OAuth scope necessary to access the Supervision server.
const char kSupervisionScope[] =
    "https://www.googleapis.com/auth/kid.family.readonly";

GURL SupervisionServerBaseUrl() {
  GURL command_line_prefix(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kSupervisionOnboardingUrlPrefix));

  if (command_line_prefix.is_valid())
    return command_line_prefix;

  if (!command_line_prefix.is_empty())
    DLOG(ERROR) << "Supervision server base URL prefix is invalid: "
                << command_line_prefix.possibly_invalid_spec();

  return GURL(kSupervisionServerUrlPrefix);
}

}  // namespace

OnboardingFlowModel::OnboardingFlowModel(Profile* profile,
                                         OnboardingDelegate* delegate)
    : profile_(profile), delegate_(delegate) {}

OnboardingFlowModel::~OnboardingFlowModel() = default;

void OnboardingFlowModel::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void OnboardingFlowModel::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void OnboardingFlowModel::StartWithWebviewHost(
    mojom::OnboardingWebviewHostPtr webview_host) {
  webview_host_ = std::move(webview_host);

  LoadStep(Step::kStart);
}

void OnboardingFlowModel::HandleAction(mojom::OnboardingAction action) {
  switch (action) {
    case mojom::OnboardingAction::kShowNextPage:
      ShowNextPage();
      return;
    case mojom::OnboardingAction::kShowPreviousPage:
      ShowPreviousPage();
      return;
    case mojom::OnboardingAction::kSkipFlow:
      SkipFlow();
      return;
  }
}

void OnboardingFlowModel::ExitFlow(ExitReason reason) {
  DCHECK(webview_host_);

  for (auto& observer : observer_list_) {
    observer.WillExitFlow(current_step_, reason);
  }

  webview_host_ = nullptr;
  if (reason == ExitReason::kUserReachedEnd) {
    delegate_->FinishOnboarding();
    return;
  }

  delegate_->SkipOnboarding();
}

mojom::OnboardingWebviewHost& OnboardingFlowModel::GetWebviewHost() {
  DCHECK(webview_host_);
  return *webview_host_;
}

//------------------------------------------------------------------------------
// Private methods

mojom::OnboardingPagePtr OnboardingFlowModel::MakePage(
    Step step,
    const std::string& access_token) {
  auto page = mojom::OnboardingPage::New();
  page->access_token = access_token;
  page->allowed_urls_prefix = SupervisionServerBaseUrl().spec();

  std::string relative_page_url;
  switch (step) {
    case Step::kStart:
      page->custom_header_name = kExperimentHeaderName;
      relative_page_url = kOnboardingStartPageRelativeUrl;
      break;
    case Step::kDetails:
      relative_page_url = kOnboardingDetailsPageRelativeUrl;
      break;
    case Step::kAllSet:
      relative_page_url = kOnboardingAllSetPageRelativeUrl;
      break;
  }

  page->url = SupervisionServerBaseUrl().Resolve(relative_page_url);
  return page;
}

void OnboardingFlowModel::ShowNextPage() {
  switch (current_step_) {
    case Step::kStart:
      LoadStep(Step::kDetails);
      return;
    case Step::kDetails:
      LoadStep(Step::kAllSet);
      return;
    case Step::kAllSet:
      ExitFlow(ExitReason::kUserReachedEnd);
      return;
  }
}

void OnboardingFlowModel::ShowPreviousPage() {
  switch (current_step_) {
    case Step::kStart:
      NOTREACHED();
      return;
    case Step::kDetails:
      LoadStep(Step::kStart);
      return;
    case Step::kAllSet:
      LoadStep(Step::kDetails);
      return;
  }
}

void OnboardingFlowModel::SkipFlow() {
  switch (current_step_) {
    case Step::kStart:
      ExitFlow(ExitReason::kFlowSkipped);
      return;
    case Step::kDetails:
    case Step::kAllSet:
      NOTREACHED();
      return;
  }
}

void OnboardingFlowModel::LoadStep(Step step) {
  current_step_ = step;

  for (auto& observer : observer_list_) {
    observer.StepStartedLoading(current_step_);
  }

  OAuth2TokenService::ScopeSet scopes{kSupervisionScope};

  // base::Unretained is safe here since |access_token_fetcher_| is owned by
  // |this|.
  access_token_fetcher_ =
      std::make_unique<identity::PrimaryAccountAccessTokenFetcher>(
          "supervision_onboarding_flow",
          IdentityManagerFactory::GetForProfile(profile_), scopes,
          base::BindOnce(&OnboardingFlowModel::AccessTokenCallback,
                         base::Unretained(this)),
          identity::PrimaryAccountAccessTokenFetcher::Mode::kImmediate);
}

void OnboardingFlowModel::AccessTokenCallback(
    GoogleServiceAuthError error,
    identity::AccessTokenInfo access_token_info) {
  DCHECK(webview_host_);
  if (error.state() != GoogleServiceAuthError::NONE) {
    ExitFlow(ExitReason::kAuthError);
    return;
  }

  webview_host_->LoadPage(MakePage(current_step_, access_token_info.token),
                          base::BindOnce(&OnboardingFlowModel::LoadPageCallback,
                                         base::Unretained(this)));
}

void OnboardingFlowModel::LoadPageCallback(
    mojom::OnboardingLoadPageResultPtr result) {
  DCHECK(webview_host_);

  if (result->net_error != net::Error::OK) {
    // TODO(crbug.com/958995): Fail here more gracefully. We should provide a
    // way to retry the fetch if the error is recoverable.
    ExitFlow(ExitReason::kWebviewNetworkError);
    return;
  }

  bool has_experiment =
      result->custom_header_value.has_value() &&
      base::EqualsCaseInsensitiveASCII(result->custom_header_value.value(),
                                       kDeviceOnboardingExperimentName);

  // Only the start step requires the experiment.
  if (current_step_ == Step::kStart && !has_experiment) {
    ExitFlow(ExitReason::kUserNotEligible);
    return;
  }

  for (auto& observer : observer_list_) {
    observer.StepFinishedLoading(current_step_);
  }
}

}  // namespace supervision
}  // namespace chromeos
