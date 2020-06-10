// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/identifiability_study_participation.h"

#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"

namespace blink {

bool IsUserInIdentifiabilityStudy() {
  return IdentifiabilityStudySettings::Get()->IsActive();
}

}  // namespace blink
