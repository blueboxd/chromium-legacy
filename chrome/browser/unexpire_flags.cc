// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/unexpire_flags.h"

#include "base/no_destructor.h"
#include "chrome/browser/expired_flags_list.h"
#include "chrome/browser/unexpire_flags_gen.h"
#include "chrome/common/chrome_version.h"

namespace flags {

namespace {

class FlagPredicateSingleton {
 public:
  FlagPredicateSingleton() = default;
  ~FlagPredicateSingleton() = default;

  static const testing::FlagPredicate& GetPredicate() {
    return GetInstance()->predicate_;
  }
  static void SetPredicate(testing::FlagPredicate predicate) {
    GetInstance()->predicate_ = predicate;
  }

 private:
  static FlagPredicateSingleton* GetInstance() {
    static base::NoDestructor<FlagPredicateSingleton> instance;
    return instance.get();
  }

  testing::FlagPredicate predicate_;
};

}  // namespace

bool IsFlagExpired(const char* internal_name) {
  constexpr int kChromeVersion[] = {CHROME_VERSION};
  constexpr int kChromeVersionMajor = kChromeVersion[0];

  if (FlagPredicateSingleton::GetPredicate())
    return FlagPredicateSingleton::GetPredicate().Run(internal_name);

  for (int i = 0; kExpiredFlags[i].name; ++i) {
    const ExpiredFlag* f = &kExpiredFlags[i];
    if (strcmp(f->name, internal_name))
      continue;

    // To keep the size of the expired flags list down,
    // //tools/flags/generate_expired_flags.py doesn't emit flags with expiry
    // mstone -1; it makes no sense for these flags to be in the expiry list
    // anyway. However, if a bug did cause that to happen, and this function
    // didn't handle that case, all flags with expiration -1 would immediately
    // expire, which would be very bad. As such there's an extra error-check
    // here: a DCHECK to catch bugs in the script, and a regular if to ensure we
    // never expire flags that should never expire.
    DCHECK_NE(f->mstone, -1);
    if (f->mstone == -1)
      continue;

    const base::Feature* expiry_feature =
        GetUnexpireFeatureForMilestone(f->mstone);

    // If there's an unexpiry feature, and the unexpiry feature is *disabled*,
    // then the flag is expired. The double-negative is very unfortunate.
    if (expiry_feature)
      return !base::FeatureList::IsEnabled(*expiry_feature);
    return f->mstone < kChromeVersionMajor;
  }
  return false;
}

namespace testing {

void SetFlagExpiredPredicate(FlagPredicate predicate) {
  FlagPredicateSingleton::SetPredicate(predicate);
}

}  // namespace testing

}  // namespace flags
