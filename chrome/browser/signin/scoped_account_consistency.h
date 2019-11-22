// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SCOPED_ACCOUNT_CONSISTENCY_H_
#define CHROME_BROWSER_SIGNIN_SCOPED_ACCOUNT_CONSISTENCY_H_

#include <memory>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "components/signin/public/base/account_consistency_method.h"

// Changes the account consistency method while it is in scope. Useful for
// tests.
class ScopedAccountConsistencyMirror {
 public:
  ScopedAccountConsistencyMirror();
  ~ScopedAccountConsistencyMirror();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAccountConsistencyMirror);
};

#endif  // CHROME_BROWSER_SIGNIN_SCOPED_ACCOUNT_CONSISTENCY_H_
