// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_IDENTIFIABILITY_STUDY_PARTICIPATION_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_IDENTIFIABILITY_STUDY_PARTICIPATION_H_

namespace blink {

// Returns true if the user is participating in the identifiability study, and
// false otherwise.
//
// This method can be used to avoid computation that is only needed for the
// study, such as complex digest calculation on canvas operations; for UKM
// reporting, filtering should happen automatically.
bool IsUserInIdentifiabilityStudy();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_IDENTIFIABILITY_STUDY_PARTICIPATION_H_
