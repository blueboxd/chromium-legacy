// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.feed.library.api.host.config.DebugBehavior;
import org.chromium.chrome.browser.feed.library.common.functional.Suppliers;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.common.time.SystemClockImpl;
import org.chromium.chrome.browser.feed.library.common.ui.LayoutUtils;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler;
import org.chromium.chrome.browser.feed.library.piet.host.AssetProvider;
import org.chromium.chrome.browser.feed.library.piet.host.CustomElementProvider;
import org.chromium.chrome.browser.feed.library.piet.host.EmptyStringFormatter;
import org.chromium.chrome.browser.feed.library.piet.host.EventLogger;
import org.chromium.chrome.browser.feed.library.piet.host.HostBindingProvider;
import org.chromium.chrome.browser.feed.library.piet.host.ImageLoader;
import org.chromium.chrome.browser.feed.library.piet.host.LogDataCallback;
import org.chromium.chrome.browser.feed.library.piet.host.NullImageLoader;
import org.chromium.chrome.browser.feed.library.piet.host.NullTypefaceProvider;
import org.chromium.chrome.browser.feed.library.piet.host.StringFormatter;
import org.chromium.chrome.browser.feed.library.piet.host.ThrowingCustomElementProvider;
import org.chromium.chrome.browser.feed.library.piet.host.TypefaceProvider;

/** Manages a top-level session of Piet. */
public interface PietManager {
    static Builder builder() {
        return new Builder();
    }

    FrameAdapter createPietFrameAdapter(Supplier<ViewGroup> cardViewProducer,
            ActionHandler actionHandler, EventLogger eventLogger, Context context);

    FrameAdapter createPietFrameAdapter(Supplier<ViewGroup> cardViewProducer,
            ActionHandler actionHandler, EventLogger eventLogger, Context context,
            @Nullable LogDataCallback logDataCallback);

    void purgeRecyclerPools();

    /**
     * Builder for PietManager that provides defaults wherever possible
     * TODO(crbug.com/1029183): Builder and its ctor were made public as a work-around for a method
     * not found exception in PietManagerImplTest.
     */
    public class Builder {
        private static final NullImageLoader BLANK_IMAGE_LOADER = new NullImageLoader();
        private static final StringFormatter EMPTY_STRING_FORMATTER = new EmptyStringFormatter();
        private static final Supplier<Integer> CORNER_RADIUS_DEFAULT = Suppliers.of(0);
        private static final Supplier<Boolean> DARK_THEME_DEFAULT = Suppliers.of(false);
        private static final Supplier<Long> FADE_IMAGE_THRESHOLD_DEFAULT =
                Suppliers.of(Long.MAX_VALUE);
        private static final TypefaceProvider NULL_TYPEFACE_PROVIDER = new NullTypefaceProvider();

        private ImageLoader mImageLoader = BLANK_IMAGE_LOADER;
        private StringFormatter mStringFormatter = EMPTY_STRING_FORMATTER;
        private Supplier<Integer> mDefaultCornerRadiusSupplier = CORNER_RADIUS_DEFAULT;
        private Supplier<Boolean> mIsDarkThemeSupplier = DARK_THEME_DEFAULT;
        private Supplier<Long> mFadeImageThresholdMsSupplier = FADE_IMAGE_THRESHOLD_DEFAULT;
        private TypefaceProvider mTypefaceProvider = NULL_TYPEFACE_PROVIDER;
        private Supplier<Boolean> mIsRtLSupplier = LayoutUtils::isDefaultLocaleRtl;

        private DebugBehavior mDebugBehavior = DebugBehavior.SILENT;
        private CustomElementProvider mCustomElementProvider;
        private HostBindingProvider mHostBindingProvider;
        private Clock mClock;
        private boolean mAllowLegacyRoundedCornerImpl;
        private boolean mAllowOutlineRoundedCornerImpl;

        public Builder() {}

        public Builder setDebugBehavior(DebugBehavior debugBehavior) {
            this.mDebugBehavior = debugBehavior;
            return this;
        }

        public Builder setCustomElementProvider(CustomElementProvider customElementProvider) {
            this.mCustomElementProvider = customElementProvider;
            return this;
        }

        public Builder setHostBindingProvider(HostBindingProvider hostBindingProvider) {
            this.mHostBindingProvider = hostBindingProvider;
            return this;
        }

        public Builder setClock(Clock clock) {
            this.mClock = clock;
            return this;
        }

        /**
         * Use the rounded corner optimizations on JB/KK for better performance at the expense of
         * antialiasing.
         */
        public Builder setAllowLegacyRoundedCornerImpl(boolean allowLegacyRoundedCornerImpl) {
            this.mAllowLegacyRoundedCornerImpl = allowLegacyRoundedCornerImpl;
            return this;
        }

        /**
         * Use the clipToOutline rounded corner optimizations on L+ when all four corners are
         * rounded for better performance.
         */
        public Builder setAllowOutlineRoundedCornerImpl(boolean allowOutlineRoundedCornerImpl) {
            this.mAllowOutlineRoundedCornerImpl = allowOutlineRoundedCornerImpl;
            return this;
        }

        // AssetProvider-related setters
        public Builder setImageLoader(ImageLoader imageLoader) {
            this.mImageLoader = imageLoader;
            return this;
        }

        public Builder setStringFormatter(StringFormatter stringFormatter) {
            this.mStringFormatter = stringFormatter;
            return this;
        }

        public Builder setDefaultCornerRadius(Supplier<Integer> defaultCornerRadiusSupplier) {
            this.mDefaultCornerRadiusSupplier = defaultCornerRadiusSupplier;
            return this;
        }

        public Builder setFadeImageThresholdMs(Supplier<Long> fadeImageThresholdMsSupplier) {
            this.mFadeImageThresholdMsSupplier = fadeImageThresholdMsSupplier;
            return this;
        }

        public Builder setIsDarkTheme(Supplier<Boolean> isDarkThemeSupplier) {
            this.mIsDarkThemeSupplier = isDarkThemeSupplier;
            return this;
        }

        public Builder setIsRtL(Supplier<Boolean> isRtLSupplier) {
            this.mIsRtLSupplier = isRtLSupplier;
            return this;
        }

        public Builder setTypefaceProvider(TypefaceProvider typefaceProvider) {
            this.mTypefaceProvider = typefaceProvider;
            return this;
        }
        // End AssetProvider-related setters

        public PietManager build() {
            mCustomElementProvider = mCustomElementProvider == null
                    ? new ThrowingCustomElementProvider()
                    : mCustomElementProvider;
            mHostBindingProvider =
                    mHostBindingProvider == null ? new HostBindingProvider() : mHostBindingProvider;
            mClock = mClock == null ? new SystemClockImpl() : mClock;

            AssetProvider assetProvider = new AssetProvider(mImageLoader, mStringFormatter,
                    mDefaultCornerRadiusSupplier, mFadeImageThresholdMsSupplier,
                    mIsDarkThemeSupplier, mIsRtLSupplier, mTypefaceProvider);

            return new PietManagerImpl(mDebugBehavior, assetProvider, mCustomElementProvider,
                    mHostBindingProvider, mClock, mAllowLegacyRoundedCornerImpl,
                    mAllowOutlineRoundedCornerImpl);
        }
    }
}
