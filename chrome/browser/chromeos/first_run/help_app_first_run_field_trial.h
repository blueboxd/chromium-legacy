// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FIRST_RUN_HELP_APP_FIRST_RUN_FIELD_TRIAL_H_
#define CHROME_BROWSER_CHROMEOS_FIRST_RUN_HELP_APP_FIRST_RUN_FIELD_TRIAL_H_

class PrefRegistrySimple;
class PrefService;

namespace base {
class FeatureList;
}  // namespace base

namespace help_app_first_run_field_trial {

// Registers preferences.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Creates a field trial to control the first run experience of the help app.
// The trial is client controlled because the first run experience happens
// before a variations seed is available. No new users are being added to the
// trial at this stage.
//
// The trial group chosen on first run was persisted to local state prefs and
// is reused on subsequent runs. This keeps the behaviour of the help app stable
// between opens. Local state prefs can be reset via powerwash which will result
// in the user no longer being part of the trial. This also sends the user
// through the first-run flow again so their experience should be consistent
// from the end of the OOBE (out-of-box experience) onwards.
//
// Tracking bug for this experiment is http://b/158540665.
// TODO(b/162372066): Clean this up around M88.
void Create(base::FeatureList* feature_list, PrefService* local_state);

}  // namespace help_app_first_run_field_trial

#endif  // CHROME_BROWSER_CHROMEOS_FIRST_RUN_HELP_APP_FIRST_RUN_FIELD_TRIAL_H_
