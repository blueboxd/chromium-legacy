// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTEXT_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTEXT_H_

namespace chromeos {

// Structure that defines data that need to be passed between screens during
// WizardController flows.
class WizardContext {
 public:
  WizardContext();
  ~WizardContext();

  WizardContext(const WizardContext&) = delete;
  WizardContext& operator=(const WizardContext&) = delete;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_CONTEXT_H_
