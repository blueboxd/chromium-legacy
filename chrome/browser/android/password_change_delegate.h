// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PASSWORD_CHANGE_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_PASSWORD_CHANGE_DELEGATE_H_

// An interface used by the native side to launch the entry editor and
// perform a change on a credential record.
class PasswordChangeDelegate {
 public:
  virtual ~PasswordChangeDelegate() = default;
};

#endif  // CHROME_BROWSER_ANDROID_PASSWORD_CHANGE_DELEGATE_H_
