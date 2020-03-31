// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_webui.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen_input_methods_controller.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chromeos {

namespace {

class UserAddingScreenImpl : public UserAddingScreen {
 public:
  void Start() override;
  void Cancel() override;
  bool IsRunning() override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  static UserAddingScreenImpl* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<UserAddingScreenImpl>;
  class LoadTimeReporter : public OobeUI::Observer {
   public:
    LoadTimeReporter() : start_time_(base::TimeTicks::Now()) {}
    LoadTimeReporter(const LoadTimeReporter&) = delete;
    LoadTimeReporter& operator=(const LoadTimeReporter&) = delete;

    ~LoadTimeReporter() override {
      if (remove_observer_)
        oobe_ui_->RemoveObserver(this);
      remove_observer_ = false;
    }

    void Observe(OobeUI* oobe_ui) {
      oobe_ui_ = oobe_ui;
      oobe_ui_->AddObserver(this);
      remove_observer_ = true;
    }

   private:
    void OnCurrentScreenChanged(OobeScreenId current_screen,
                                OobeScreenId new_screen) override {
      if (new_screen != OobeScreen::SCREEN_ACCOUNT_PICKER)
        return;
      const base::TimeDelta load_time = base::TimeTicks::Now() - start_time_;
      UMA_HISTOGRAM_TIMES("ChromeOS.UserAddingScreen.LoadTime", load_time);
      remove_observer_ = false;
      oobe_ui_->RemoveObserver(this);
    }
    void OnDestroyingOobeUI() override { remove_observer_ = false; }

   private:
    const base::TimeTicks start_time_;
    OobeUI* oobe_ui_ = nullptr;
    bool remove_observer_ = false;
  };

  std::unique_ptr<LoadTimeReporter> reporter_;

  void OnDisplayHostCompletion();

  UserAddingScreenImpl();
  ~UserAddingScreenImpl() override;

  base::ObserverList<Observer>::Unchecked observers_;
  LoginDisplayHost* display_host_;

  UserAddingScreenInputMethodsController im_controller_;
};

void UserAddingScreenImpl::Start() {
  CHECK(!IsRunning());
  reporter_ = std::make_unique<LoadTimeReporter>();
  display_host_ = new chromeos::LoginDisplayHostWebUI();
  display_host_->StartUserAdding(base::BindOnce(
      &UserAddingScreenImpl::OnDisplayHostCompletion, base::Unretained(this)));
  reporter_->Observe(display_host_->GetOobeUI());

  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOGIN_SECONDARY);
  for (auto& observer : observers_)
    observer.OnUserAddingStarted();
}

void UserAddingScreenImpl::Cancel() {
  CHECK(IsRunning());
  reporter_.reset();

  display_host_->CancelUserAdding();

  // Reset wallpaper if cancel adding user from multiple user sign in page.
  if (user_manager::UserManager::Get()->IsUserLoggedIn()) {
    WallpaperControllerClient::Get()->ShowUserWallpaper(
        user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
  }
}

bool UserAddingScreenImpl::IsRunning() {
  return display_host_ != NULL;
}

void UserAddingScreenImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UserAddingScreenImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void UserAddingScreenImpl::OnDisplayHostCompletion() {
  CHECK(IsRunning());
  display_host_ = NULL;

  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  for (auto& observer : observers_)
    observer.OnUserAddingFinished();
}

// static
UserAddingScreenImpl* UserAddingScreenImpl::GetInstance() {
  return base::Singleton<UserAddingScreenImpl>::get();
}

UserAddingScreenImpl::UserAddingScreenImpl()
    : display_host_(NULL), im_controller_(this) {}

UserAddingScreenImpl::~UserAddingScreenImpl() {}

}  // anonymous namespace

UserAddingScreen::UserAddingScreen() {}
UserAddingScreen::~UserAddingScreen() {}

UserAddingScreen* UserAddingScreen::Get() {
  return UserAddingScreenImpl::GetInstance();
}

}  // namespace chromeos
