// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_STATE_CORE_SECURITY_STATE_PREF_NAMES_H_
#define COMPONENTS_SECURITY_STATE_CORE_SECURITY_STATE_PREF_NAMES_H_

namespace security_state {
namespace prefs {

// Boolean preference used to control whether mixed content should trigger a
// stricter treatment (a 'Not Secure' warning in the URL bar for all passive
// mixed content displayed, and autoupgrades to HTTPS for audio/video content).
extern const char kStricterMixedContentTreatmentEnabled[];

}  // namespace prefs
}  // namespace security_state

#endif  // COMPONENTS_SECURITY_STATE_CORE_SECURITY_STATE_PREF_NAMES_H_
