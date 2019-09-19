// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_SCREEN_WAITER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_SCREEN_WAITER_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/test/test_condition_waiter.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

namespace base {
class RunLoop;
}

namespace chromeos {

class OobeUI;

// A waiter that blocks until the target oobe screen is reached.
class OobeScreenWaiter : public OobeUI::Observer,
                         public test::TestConditionWaiter {
 public:
  explicit OobeScreenWaiter(OobeScreenId target_screen);
  ~OobeScreenWaiter() override;

  void set_no_assert_last_screen() { assert_last_screen_ = false; }
  void set_assert_next_screen() { assert_next_screen_ = true; }

  // OobeUI::Observer implementation:
  void OnCurrentScreenChanged(OobeScreenId current_screen,
                              OobeScreenId new_screen) override;
  void OnDestroyingOobeUI() override;

  // TestConditionWaiter;
  void Wait() override;

 private:
  enum class State { IDLE, WAITING_FOR_SCREEN, DONE };

  OobeUI* GetOobeUI();
  void EndWait();

  const OobeScreenId target_screen_;

  State state_ = State::IDLE;

  // If set, waiter will assert that the OOBE UI does not transition to any
  // other screen after the transition to the target screen.
  // This applies to the time period Wait() is running only.
  bool assert_last_screen_ = true;

  // If set, the watier will assert OOBE UI does not transition to any other
  // screen before transisioning to the target screen.
  bool assert_next_screen_ = false;

  ScopedObserver<OobeUI, OobeScreenWaiter> oobe_ui_observer_{this};

  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(OobeScreenWaiter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_SCREEN_WAITER_H_
