// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/test_app_banner_manager_desktop.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/installable/installable_logging.h"
#include "content/public/browser/web_contents.h"

namespace banners {

TestAppBannerManagerDesktop::TestAppBannerManagerDesktop(
    content::WebContents* web_contents)
    : AppBannerManagerDesktop(web_contents) {
  MigrateObserverListForTesting(web_contents);
}

TestAppBannerManagerDesktop::~TestAppBannerManagerDesktop() = default;

TestAppBannerManagerDesktop* TestAppBannerManagerDesktop::CreateForWebContents(
    content::WebContents* web_contents) {
  auto banner_manager =
      std::make_unique<TestAppBannerManagerDesktop>(web_contents);
  TestAppBannerManagerDesktop* result = banner_manager.get();
  web_contents->SetUserData(UserDataKey(), std::move(banner_manager));
  return result;
}

void TestAppBannerManagerDesktop::WaitForInstallableCheckTearDown() {
  base::RunLoop run_loop;
  tear_down_quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

bool TestAppBannerManagerDesktop::WaitForInstallableCheck() {
  if (!installable_.has_value()) {
    base::RunLoop run_loop;
    installable_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  return *installable_;
}

void TestAppBannerManagerDesktop::PrepareDone(base::OnceClosure on_done) {
  on_done_ = std::move(on_done);
}

AppBannerManager::State TestAppBannerManagerDesktop::state() {
  return AppBannerManager::state();
}

void TestAppBannerManagerDesktop::AwaitAppInstall() {
  base::RunLoop loop;
  on_install_ = loop.QuitClosure();
  loop.Run();
}

void TestAppBannerManagerDesktop::OnDidGetManifest(
    const InstallableData& result) {
  AppBannerManagerDesktop::OnDidGetManifest(result);

  // AppBannerManagerDesktop does not call |OnDidPerformInstallableCheck| to
  // complete the installability check in this case, instead it early exits
  // with failure.
  if (!result.errors.empty())
    SetInstallable(false);
}
void TestAppBannerManagerDesktop::OnDidPerformInstallableWebAppCheck(
    const InstallableData& result) {
  AppBannerManagerDesktop::OnDidPerformInstallableWebAppCheck(result);
  SetInstallable(result.errors.empty());
}

void TestAppBannerManagerDesktop::ResetCurrentPageData() {
  AppBannerManagerDesktop::ResetCurrentPageData();
  installable_.reset();
  if (tear_down_quit_closure_)
    std::move(tear_down_quit_closure_).Run();
}

void TestAppBannerManagerDesktop::OnInstall(blink::mojom::DisplayMode display) {
  AppBannerManager::OnInstall(display);
  if (on_install_)
    std::move(on_install_).Run();
}

void TestAppBannerManagerDesktop::DidFinishCreatingWebApp(
    const web_app::AppId& app_id,
    web_app::InstallResultCode code) {
  AppBannerManagerDesktop::DidFinishCreatingWebApp(app_id, code);
  OnFinished();
}

void TestAppBannerManagerDesktop::UpdateState(AppBannerManager::State state) {
  AppBannerManager::UpdateState(state);

  if (state == AppBannerManager::State::PENDING_ENGAGEMENT ||
      state == AppBannerManager::State::PENDING_PROMPT ||
      state == AppBannerManager::State::COMPLETE) {
    OnFinished();
  }
}

void TestAppBannerManagerDesktop::SetInstallable(bool installable) {
  DCHECK(!installable_.has_value());
  installable_ = installable;
  if (installable_quit_closure_)
    std::move(installable_quit_closure_).Run();
}

void TestAppBannerManagerDesktop::OnFinished() {
  if (on_done_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(on_done_));
  }
}

}  // namespace banners
