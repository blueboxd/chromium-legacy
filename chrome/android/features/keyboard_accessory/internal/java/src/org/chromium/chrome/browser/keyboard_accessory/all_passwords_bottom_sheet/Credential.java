// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import org.chromium.base.annotations.CalledByNative;

/**
 * This class holds the data used to represent a selectable credential in the
 * AllPasswordsBottomSheet.
 */
class Credential {
    private final String mUsername;
    private final String mPassword;
    private final String mFormattedUsername;
    private final String mOriginUrl;
    private final boolean mIsPublicSuffixMatch;
    private final boolean mIsAffiliationBasedMatch;

    /**
     * @param username Username shown to the user.
     * @param password Password shown to the user.
     * @param originUrl Origin URL shown to the user in case this credential is a PSL match.
     * @param isPublicSuffixMatch Indicating whether the credential is a PSL match.
     * @param isAffiliationBasedMatch Indicating whether the credential is an affiliation based
     * match (i.e. whether it is an Android credential).
     */
    Credential(String username, String password, String formattedUsername, String originUrl,
            boolean isPublicSuffixMatch, boolean isAffiliationBasedMatch) {
        assert originUrl != null : "Credential origin is null! Pass an empty one instead.";
        mUsername = username;
        mPassword = password;
        mFormattedUsername = formattedUsername;
        mOriginUrl = originUrl;
        mIsPublicSuffixMatch = isPublicSuffixMatch;
        mIsAffiliationBasedMatch = isAffiliationBasedMatch;
    }

    @CalledByNative
    String getUsername() {
        return mUsername;
    }

    @CalledByNative
    String getPassword() {
        return mPassword;
    }

    String getFormattedUsername() {
        return mFormattedUsername;
    }

    @CalledByNative
    String getOriginUrl() {
        return mOriginUrl;
    }

    @CalledByNative
    boolean isPublicSuffixMatch() {
        return mIsPublicSuffixMatch;
    }

    @CalledByNative
    boolean isAffiliationBasedMatch() {
        return mIsAffiliationBasedMatch;
    }
}
