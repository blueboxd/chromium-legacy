// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller;

import org.chromium.base.Promise;
import org.chromium.chrome.browser.browserservices.Origin;

/**
 * A Delegate for the {@link Verifier} that provides implementation specific to
 * Trusted Web Activities, WebAPKs or A2HS as appropriate.
 */
public interface VerifierDelegate {
    // TODO(peconn): Make distinction between verify and wasPreviouslyVerified more clear.
    /** Asynchronously checks whether the given Origin is verified. */
    Promise<Boolean> verify(Origin origin);

    /** Synchronously checks whether verification was successful for the given Origin. */
    boolean wasPreviouslyVerified(Origin origin);

    // TODO(peconn): Get rid of this method.
    /** Gets the package name of the connected app. */
    String getClientPackageName();
}
