// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/invalidator_state.h"


namespace syncer {

const char* InvalidatorStateToString(InvalidatorState state) {
  switch (state) {
    case TRANSIENT_INVALIDATION_ERROR:
      return "TRANSIENT_INVALIDATION_ERROR";
    case INVALIDATION_CREDENTIALS_REJECTED:
      return "INVALIDATION_CREDENTIALS_REJECTED";
    case INVALIDATIONS_ENABLED:
      return "INVALIDATIONS_ENABLED";
    case INVALIDATOR_SHUTTING_DOWN:
      return "INVALIDATOR_SHUTTING_DOWN";
    case SUBSCRIPTION_FAILURE:
      return "SUBSCRIPTION_FAILURE";
    case STOPPED:
      return "STOPPED";
    case NOT_STARTED_NO_ACTIVE_ACCOUNT:
      return "NOT_STARTED_NO_ACTIVE_ACCOUNT";
    case NOT_STARTED_NO_REFRESH_TOKEN:
      return "NOT_STARTED_NO_REFRESH_TOKEN";
  }
}

}  // namespace syncer
