// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.os.Bundle;

import java.util.Map;

/**
 * Interface for base module to start the autofill assistant
 * experience in dynamic feature module.
 */
interface AutofillAssistantModuleEntry {
    /**
     * Launches Autofill Assistant on the current web contents, expecting autostart. If {@code
     * skipOnboarding} is false, the onboarding will first be shown and the Autofill Assistant will
     * start only if the user accepts to proceed.
     */
    void start(boolean skipOnboarding, String initialUrl, Map<String, String> parameters,
            String experimentIds, Bundle intentExtras);
}