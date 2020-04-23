// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/assistant_optin_flow_screen.h"

#include "chrome/browser/chromeos/assistant/assistant_util.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/assistant_optin_flow_screen_handler.h"
#include "chromeos/assistant/buildflags.h"
#include "chromeos/constants/chromeos_features.h"

namespace chromeos {
namespace {

constexpr const char kFlowFinished[] = "flow-finished";

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
bool g_libassistant_enabled = true;
#else
bool g_libassistant_enabled = false;
#endif

}  // namespace

// static
AssistantOptInFlowScreen* AssistantOptInFlowScreen::Get(
    ScreenManager* manager) {
  return static_cast<AssistantOptInFlowScreen*>(
      manager->GetScreen(AssistantOptInFlowScreenView::kScreenId));
}

AssistantOptInFlowScreen::AssistantOptInFlowScreen(
    AssistantOptInFlowScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(AssistantOptInFlowScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  if (view_)
    view_->Bind(this);
}

AssistantOptInFlowScreen::~AssistantOptInFlowScreen() {
  if (view_)
    view_->Unbind();
}

void AssistantOptInFlowScreen::ShowImpl() {
  if (!view_)
    return;

  if (chrome_user_manager_util::IsPublicSessionOrEphemeralLogin()) {
    exit_callback_.Run();
    return;
  }

  if (!g_libassistant_enabled) {
    exit_callback_.Run();
    return;
  }

  if (::assistant::IsAssistantAllowedForProfile(
          ProfileManager::GetActiveUserProfile()) ==
      ash::mojom::AssistantAllowedState::ALLOWED) {
    view_->Show();
    return;
  }
  exit_callback_.Run();
}

void AssistantOptInFlowScreen::HideImpl() {
  if (view_)
    view_->Hide();
}

void AssistantOptInFlowScreen::OnViewDestroyed(
    AssistantOptInFlowScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

// static
std::unique_ptr<base::AutoReset<bool>>
AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting() {
  return std::make_unique<base::AutoReset<bool>>(&g_libassistant_enabled, true);
}

void AssistantOptInFlowScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kFlowFinished)
    exit_callback_.Run();
  else
    BaseScreen::OnUserAction(action_id);
}

}  // namespace chromeos
