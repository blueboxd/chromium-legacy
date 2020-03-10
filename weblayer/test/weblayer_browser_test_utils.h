// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_TEST_WEBLAYER_BROWSER_TEST_UTILS_H_
#define WEBLAYER_TEST_WEBLAYER_BROWSER_TEST_UTILS_H_

#include "base/callback_forward.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/values.h"

class GURL;

namespace autofill {
struct FormData;
}

namespace weblayer {
class Shell;
class Tab;

// Navigates |shell| to |url| and wait for completed navigation.
void NavigateAndWaitForCompletion(const GURL& url, Shell* shell);

void NavigateAndWaitForCompletion(const GURL& url, Tab* tab);

// Navigates |shell| to |url| and wait for failed navigation.
void NavigateAndWaitForFailure(const GURL& url, Shell* shell);

// Initiates navigation to |url| in |tab| and waits for it to start.
void NavigateAndWaitForStart(const GURL& url, Tab* tab);

// Executes |script| in |shell| and returns the result.
base::Value ExecuteScript(Shell* shell,
                          const std::string& script,
                          bool use_separate_isolate);

// Executes |script| in |shell| with a user gesture. Useful for tests of
// functionality that gates action on a user gesture having occurred.
// Differs from ExecuteScript() as follows:
// - Does not notify the caller of the result as the underlying implementation
//   does not. Thus, unlike the above, the caller of this function will need to
//   explicitly listen *after* making this call for any expected event to
//   occur.
// - Does not allow running in a separate isolate as the  machinery for
//   setting a user gesture works only in the main isolate.
void ExecuteScriptWithUserGesture(Shell* shell, const std::string& script);

/// Gets the title of the current webpage in |shell|.
const base::string16& GetTitle(Shell* shell);

// Sets up the autofill system to be one that simply forwards detected forms to
// the passed-in callback.
void InitializeAutofillWithEventForwarding(
    Shell* shell,
    const base::RepeatingCallback<void(const autofill::FormData&)>&
        on_received_form_data);

}  // namespace weblayer

#endif  // WEBLAYER_TEST_WEBLAYER_BROWSER_TEST_UTILS_H_
