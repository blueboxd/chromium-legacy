// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.Bundle;

import androidx.annotation.ColorRes;
import androidx.annotation.NonNull;

import org.chromium.weblayer_private.interfaces.UrlBarOptionsKeys;

/**
 * Class containing options to tweak the URL bar.
 */
public final class UrlBarOptions {
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
            mOptions.putFloat(UrlBarOptionsKeys.URL_TEXT_SIZE, textSize);
            return this;
        }

        /**
         * Specifies whether the URL text in the URL bar should also show Page Info UI on click.
         * By default, only the security status icon does so.
         */
        @NonNull
        public Builder showPageInfoWhenTextIsClicked() {
            mOptions.putBoolean(UrlBarOptionsKeys.SHOW_PAGE_INFO_WHEN_URL_TEXT_CLICKED, true);
            return this;
        }

        /**
         * Sets the color of the URL bar text.
         *
         * @param textColor The color for the Url bar text.
         */
        @NonNull
        public Builder setTextColor(@ColorRes int textColor) {
            mOptions.putInt(UrlBarOptionsKeys.URL_TEXT_COLOR, textColor);
            return this;
        }

        /**
         * Sets the color of the URL bar security status icon.
         *
         * @param iconColor The color for the Url bar icon.
         */
        @NonNull
        public Builder setIconColor(@ColorRes int iconColor) {
            mOptions.putInt(UrlBarOptionsKeys.URL_ICON_COLOR, iconColor);
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
        return mOptions.getFloat(UrlBarOptionsKeys.URL_TEXT_SIZE);
    }
}
