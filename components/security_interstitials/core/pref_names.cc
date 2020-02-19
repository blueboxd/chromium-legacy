// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/pref_names.h"

namespace prefs {

// Stores counts and timestamps of SSL certificate errors that have occurred.
// When the same error recurs within some period of time, a message is added to
// the SSL interstitial.
const char kRecurrentSSLInterstitial[] = "profile.ssl_recurrent_interstitial";

}  // namespace prefs
