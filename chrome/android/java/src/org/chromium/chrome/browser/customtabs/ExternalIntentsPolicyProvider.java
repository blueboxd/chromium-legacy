// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.chrome.browser.dependency_injection.ActivityScope;

import javax.inject.Inject;

import androidx.annotation.Nullable;

/**
 * This class is used to decide whether a navigation to a certain URL should be kept within Chrome
 * (as opposed to considering Android Apps that can handle it).
 */
@ActivityScope
public class ExternalIntentsPolicyProvider {
    @Nullable private PolicyCriteria mPolicyCriteria;

    @Inject
    ExternalIntentsPolicyProvider() {}

    /**
     * An interface that allows specifying whether a navigation to a given url should be kept within
     * Chrome.
     */
    public interface PolicyCriteria {
        /** Whether a navigation to the given url should be kept within Chrome. */
        boolean shouldIgnoreExternalIntentHandlers(String url);
    }

    public void setPolicyCriteria(PolicyCriteria criteria) {
        assert mPolicyCriteria == null
                : "ExternalIntentsPolicyProvider can't handle multiple policies";

        mPolicyCriteria = criteria;
    }

    boolean shouldIgnoreExternalIntentHandlers(String url) {
        return mPolicyCriteria != null && mPolicyCriteria.shouldIgnoreExternalIntentHandlers(url);
    }
}
