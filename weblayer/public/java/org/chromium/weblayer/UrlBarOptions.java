// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.Bundle;

import androidx.annotation.NonNull;

/**
 * Class containing options to tweak the URL bar.
 */
public final class UrlBarOptions {
    // Keep in sync with the constants in UrlBarControllerImpl.java
    private static final String URL_TEXT_SIZE = "UrlTextSize";

    public static Builder builder() {
        return new Builder();
    }

    private Bundle mOptions;

    /**
     * A Builder class to help create UrlBarOptions.
     */
    public static final class Builder {
        private Bundle mOptions;

        private Builder() {
            mOptions = new Bundle();
        }

        Bundle getBundle() {
            return mOptions;
        }

        /**
         * Sets the text size of the URL bar.
         *
         * @param textSize The desired size of the URL bar text in scalable pixels.
         * The default is 14.0F and the minimum allowed size is 5.0F.
         */
        @NonNull
        public Builder setTextSizeSP(float textSize) {
            mOptions.putFloat(URL_TEXT_SIZE, textSize);
            return this;
        }

        /**
         * Builds a UrlBarOptions object.
         */
        @NonNull
        public UrlBarOptions build() {
            return new UrlBarOptions(this);
        }
    }

    private UrlBarOptions(Builder builder) {
        mOptions = builder.getBundle();
    }

    /**
     * Gets the URL bar options as a Bundle.
     */
    Bundle getBundle() {
        return mOptions;
    }

    /**
     * Gets the text size of the URL bar text in scalable pixels.
     */
    public float getTextSizeSP() {
        return mOptions.getFloat(URL_TEXT_SIZE);
    }
}
