// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import android.os.Bundle;
import android.os.CancellationSignal;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * Utilities for writing tests that check for or perform direct actions on an activity.
 */
public class DirectActionTestUtils {
    /** Perform a direct action, with the given name. */
    public static void callOnPerformDirectActions(
            ChromeActivity activity, String actionId, Callback<Bundle> callback) {
        // This method is not taking a Consumer to avoid issues with tests running against Android
        // API < 24 not even being able to load the test class.

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            activity.onPerformDirectAction(actionId, Bundle.EMPTY, new CancellationSignal(),
                    (r) -> callback.onResult((Bundle) r));
        });
    }

    /** Gets the list of direct actions. */
    public static List<String> callOnGetDirectActions(ChromeActivity activity) {
        List<String> directActions = new ArrayList<>();

        // onGetDirectActions reports a List<String> because that's what FakeDirectActionReporter
        // creates.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            activity.onGetDirectActions(new CancellationSignal(),
                    (actions) -> directActions.addAll((List<String>) actions));
        });
        return directActions;
    }

    private DirectActionTestUtils() {
        // This is a utility class; it is not meant to be instantiated.
    }
}
