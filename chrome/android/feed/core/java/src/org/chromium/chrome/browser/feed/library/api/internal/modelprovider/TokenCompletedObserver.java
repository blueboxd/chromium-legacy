// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.modelprovider;

/** Observes for when the Token fetch completes, producing a new cursor. */
public interface TokenCompletedObserver {
    /** Called when the token processing has completed. */
    void onTokenCompleted(TokenCompleted tokenCompleted);

    /**
     * This is called in the event of an error. For example, if we are making a pagination request
     * and it fails due to network connectivity issues, this event will indicate the error.
     */
    void onError(ModelError modelError);
}
