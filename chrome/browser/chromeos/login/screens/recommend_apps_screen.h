// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_SCREEN_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps/recommend_apps_fetcher_delegate.h"

namespace base {
class Value;
}

namespace chromeos {

class RecommendAppsFetcher;
class RecommendAppsScreenView;
class ScreenManager;

// This is Recommend Apps screen that is displayed as a part of user first
// sign-in flow.
class RecommendAppsScreen : public BaseScreen,
                            public RecommendAppsFetcherDelegate {
 public:
  enum class Result { SELECTED, SKIPPED, NOT_APPLICABLE, LOAD_ERROR };

  static std::string GetResultString(Result result);

  static RecommendAppsScreen* Get(ScreenManager* manager);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  RecommendAppsScreen(RecommendAppsScreenView* view,
                      const ScreenExitCallback& exit_callback);
  ~RecommendAppsScreen() override;

  // Called when the user skips the Recommend Apps screen.
  void OnSkip();

  // Called when the user tries to reload the screen.
  void OnRetry();

  // Called when the user Install the selected apps.
  void OnInstall();

  // Called when the view is destroyed so there is no dead reference to it.
  void OnViewDestroyed(RecommendAppsScreenView* view);

  // RecommendAppsFetcherDelegate:
  void OnLoadSuccess(const base::Value& app_list) override;
  void OnLoadError() override;
  void OnParseResponseError() override;

  // BaseScreen:
  bool MaybeSkip() override;

  void set_exit_callback_for_testing(ScreenExitCallback exit_callback) {
    exit_callback_ = exit_callback;
  }

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;

  RecommendAppsScreenView* view_;
  ScreenExitCallback exit_callback_;

  std::unique_ptr<RecommendAppsFetcher> recommend_apps_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(RecommendAppsScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_SCREEN_H_
