// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller;

import org.chromium.base.Promise;
import org.chromium.chrome.browser.browserservices.Origin;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * A {@link VerifierDelegate} for testing.
 */
class TestVerifierDelegate implements VerifierDelegate {
    private final Set<Origin> mPreviouslyVerifiedOrigins = new HashSet<>();
    private final Map<Origin, Promise<Boolean>> mPendingVerifications = new HashMap<>();

    private final String mPackageName;

    TestVerifierDelegate(String packageName) {
        mPackageName = packageName;
    }

    @Override
    public Promise<Boolean> verify(Origin origin) {
        Promise<Boolean> promise = new Promise<>();
        mPendingVerifications.put(origin, promise);
        return promise;
    }

    @Override
    public boolean wasPreviouslyVerified(Origin origin) {
        return mPreviouslyVerifiedOrigins.contains(origin);
    }

    @Override
    public String getClientPackageName() {
        return mPackageName;
    }

    public void passVerification(Origin origin) {
        completeVerification(origin, true);
    }

    public void failVerification(Origin origin) {
        completeVerification(origin, false);
    }

    private void completeVerification(Origin origin, boolean success) {
        if (mPendingVerifications.get(origin) == null) return;

        mPendingVerifications.get(origin).fulfill(success);
        mPendingVerifications.remove(origin);

        if (success) mPreviouslyVerifiedOrigins.add(origin);
    }

    public boolean hasPendingVerification(Origin origin) {
        return mPendingVerifications.containsKey(origin);
    }

    public boolean hasAnyPendingVerifications() {
        return mPendingVerifications.size() != 0;
    }
}
