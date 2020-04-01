// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/scheduling.h"
#include "base/time/time.h"

namespace feed {

base::TimeDelta GetUserClassTriggerThreshold(UserClass user_class,
                                             TriggerType trigger) {
  switch (user_class) {
    case UserClass::kRareSuggestionsViewer:
      switch (trigger) {
        case TriggerType::kNtpShown:
          return base::TimeDelta::FromHours(4);
        case TriggerType::kForegrounded:
          return base::TimeDelta::FromHours(24);
        case TriggerType::kFixedTimer:
          return base::TimeDelta::FromHours(96);
      }
    case UserClass::kActiveSuggestionsViewer:
      switch (trigger) {
        case TriggerType::kNtpShown:
          return base::TimeDelta::FromHours(4);
        case TriggerType::kForegrounded:
          return base::TimeDelta::FromHours(24);
        case TriggerType::kFixedTimer:
          return base::TimeDelta::FromHours(48);
      }
    case UserClass::kActiveSuggestionsConsumer:
      switch (trigger) {
        case TriggerType::kNtpShown:
          return base::TimeDelta::FromHours(1);
        case TriggerType::kForegrounded:
          return base::TimeDelta::FromHours(12);
        case TriggerType::kFixedTimer:
          return base::TimeDelta::FromHours(24);
      }
  }
}

bool ShouldWaitForNewContent(UserClass user_class,
                             bool has_content,
                             base::TimeDelta content_age) {
  return !has_content ||
         content_age > GetUserClassTriggerThreshold(user_class,
                                                    TriggerType::kForegrounded);
}

}  // namespace feed
