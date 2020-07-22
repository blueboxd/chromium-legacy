// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_LACROS_CHROME_SERVICE_DELEGATE_H_
#define CHROMEOS_LACROS_LACROS_CHROME_SERVICE_DELEGATE_H_

namespace chromeos {

// Interface to inject Chrome dependent behavior into LacrosChromeServiceImpl
// to split the dependency.
class LacrosChromeServiceDelegate {
 public:
  virtual ~LacrosChromeServiceDelegate() = default;

  // Opens a new browser window.
  virtual void NewWindow() = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_LACROS_LACROS_CHROME_SERVICE_DELEGATE_H_
