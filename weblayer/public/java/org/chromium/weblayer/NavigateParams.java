// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;

/**
 * Parameters for {@link NavigationController#navigate}.
 *
 * @since 83
 */
public class NavigateParams {
    private org.chromium.weblayer_private.interfaces.NavigateParams mInterfaceParams =
            new org.chromium.weblayer_private.interfaces.NavigateParams();

    /**
     * A Builder class to help create NavigateParams.
     */
    public static final class Builder {
        private NavigateParams mParams;

        /**
         * Constructs a new Builder.
         */
        public Builder() {
            mParams = new NavigateParams();
        }

        /**
         * Builds the NavigateParams.
         */
        @NonNull
        public NavigateParams build() {
            return mParams;
        }

        /**
         * @param replace Indicates whether the navigation should replace the current navigation
         *         entry in the history stack. False by default.
         */
        @NonNull
        public Builder setShouldReplaceCurrentEntry(boolean replace) {
            mParams.mInterfaceParams.mShouldReplaceCurrentEntry = replace;
            return this;
        }
    }

    org.chromium.weblayer_private.interfaces.NavigateParams toInterfaceParams() {
        return mInterfaceParams;
    }
}
