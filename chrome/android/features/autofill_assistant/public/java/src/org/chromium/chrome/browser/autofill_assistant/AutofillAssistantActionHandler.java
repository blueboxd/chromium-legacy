// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.os.Bundle;

import org.chromium.base.Callback;

import java.util.Set;

/**
 * Interface that provides implementation for AA actions, triggered by direct actions.
 */
public interface AutofillAssistantActionHandler {
    /**
     * Returns the names of available AA actions.
     *
     * <p>This method starts AA on the current tab, if necessary, and waits for the first results.
     * Once AA is started, the results are reported immediately.
     *
     * @param userName name of the user to use when sending RPCs. Might be empty.
     * @param experimentIds comma-separated set of experiment ids. Might be empty
     * @param arguments extra arguments to include into the RPC. Might be empty.
     * @param callback callback to report the result to
     */
    void listActions(String userName, String experimentIds, Bundle arguments,
            Callback<Set<String>> callback);

    /** Performs onboarding and returns the result to the callback. */
    void performOnboarding(String experimentIds, Callback<Boolean> callback);

    /**
     * Performs an AA action.
     *
     * <p>If this method returns {@code true}, a definition for the action was successfully started.
     * It can still fail later, and the failure will be reported to the UI.
     *
     * @param name action name, might be empty to autostart
     * @param experimentIds comma-separated set of experiment ids. Might be empty.
     * @param arguments extra arguments to pass to the action. Might be empty.
     * @param callback to report the result to
     */
    void performAction(
            String name, String experimentIds, Bundle arguments, Callback<Boolean> callback);
}
