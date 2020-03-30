// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/recommend_apps_screen.h"

#include "chrome/browser/chromeos/login/screens/recommend_apps/recommend_apps_fetcher.h"
#include "chrome/browser/ui/webui/chromeos/login/recommend_apps_screen_handler.h"

namespace chromeos {

// static
std::string RecommendAppsScreen::GetResultString(Result result) {
  switch (result) {
    case Result::SELECTED:
      return "Selected";
    case Result::SKIPPED:
      return "Skipped";
  }
}

RecommendAppsScreen::RecommendAppsScreen(
    RecommendAppsScreenView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(RecommendAppsScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);

  view_->Bind(this);
}

RecommendAppsScreen::~RecommendAppsScreen() {
  if (view_)
    view_->Bind(nullptr);
}

void RecommendAppsScreen::OnSkip() {
  exit_callback_.Run(Result::SKIPPED);
}

void RecommendAppsScreen::OnRetry() {
  recommend_apps_fetcher_->Retry();
}

void RecommendAppsScreen::OnInstall() {
  exit_callback_.Run(Result::SELECTED);
}

void RecommendAppsScreen::OnViewDestroyed(RecommendAppsScreenView* view) {
  DCHECK_EQ(view, view_);
  view_ = nullptr;
}

void RecommendAppsScreen::ShowImpl() {
  view_->Show();

  recommend_apps_fetcher_ = RecommendAppsFetcher::Create(this);
  recommend_apps_fetcher_->Start();
}

void RecommendAppsScreen::HideImpl() {
  view_->Hide();
}

void RecommendAppsScreen::OnLoadSuccess(const base::Value& app_list) {
  if (view_)
    view_->OnLoadSuccess(app_list);
}

void RecommendAppsScreen::OnLoadError() {
  if (view_)
    view_->OnLoadError();
}

void RecommendAppsScreen::OnParseResponseError() {
  if (view_)
    view_->OnParseResponseError();
}

}  // namespace chromeos
