// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_LIMIT_INTERFACE_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_LIMIT_INTERFACE_H_

#include <string>

#include "base/time/time.h"

class Profile;

namespace chromeos {
namespace app_time {

// Interface of the object controlling UI for web time limits feature.
class WebTimeLimitInterface {
 public:
  // Factory method that returns object controlling UI for web time limits
  // feature. Provided to reduce the dependencies between API consumer and child
  // user related code. WebTimeLimitInterface object has a lifetime of a
  // KeyedService.
  static WebTimeLimitInterface* Get(Profile* profile);

  virtual ~WebTimeLimitInterface();

  // Blocks access to Chrome and web apps. Should be called when the daily
  // time limit is reached. Calling it multiple times is safe.
  // |app_id| identifies web application active when limit was reached.
  // Currently the web time limit is shared between all PWAs and Chrome and all
  // of them will be paused regardless |app_id|.
  virtual void PauseWebActivity(const std::string& app_id) = 0;

  // Resumes access to Chrome and web apps. Should be called when the daily time
  // limit is lifted. Calling it multiple times is safe. Subsequent calls will
  // be ignored.
  // |app_id| identifies web application active when limit was reached.
  // Currently the web time limit is shared between all PWAs and Chrome and all
  // of them will be resumed regardless |app_id|.
  virtual void ResumeWebActivity(const std::string& app_id) = 0;
};

}  // namespace app_time
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_LIMIT_INTERFACE_H_
