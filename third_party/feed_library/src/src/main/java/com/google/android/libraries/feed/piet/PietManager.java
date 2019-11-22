// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import android.content.Context;
import android.view.ViewGroup;

import com.google.android.libraries.feed.api.host.config.DebugBehavior;
import com.google.android.libraries.feed.common.functional.Supplier;
import com.google.android.libraries.feed.common.functional.Suppliers;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.common.time.SystemClockImpl;
import com.google.android.libraries.feed.common.ui.LayoutUtils;
import com.google.android.libraries.feed.piet.host.ActionHandler;
import com.google.android.libraries.feed.piet.host.AssetProvider;
import com.google.android.libraries.feed.piet.host.CustomElementProvider;
import com.google.android.libraries.feed.piet.host.EmptyStringFormatter;
import com.google.android.libraries.feed.piet.host.EventLogger;
import com.google.android.libraries.feed.piet.host.HostBindingProvider;
import com.google.android.libraries.feed.piet.host.ImageLoader;
import com.google.android.libraries.feed.piet.host.LogDataCallback;
import com.google.android.libraries.feed.piet.host.NullImageLoader;
import com.google.android.libraries.feed.piet.host.NullTypefaceProvider;
import com.google.android.libraries.feed.piet.host.StringFormatter;
import com.google.android.libraries.feed.piet.host.ThrowingCustomElementProvider;
import com.google.android.libraries.feed.piet.host.TypefaceProvider;

/** Manages a top-level session of Piet. */
public interface PietManager {
    static Builder builder() {
        return new Builder();
    }

    FrameAdapter createPietFrameAdapter(Supplier</*@Nullable*/ ViewGroup> cardViewProducer,
            ActionHandler actionHandler, EventLogger eventLogger, Context context);

    FrameAdapter createPietFrameAdapter(Supplier</*@Nullable*/ ViewGroup> cardViewProducer,
            ActionHandler actionHandler, EventLogger eventLogger, Context context,
            /*@Nullable*/ LogDataCallback logDataCallback);

    void purgeRecyclerPools();

    /** Builder for PietManager that provides defaults wherever possible */
    class Builder {
        private static final NullImageLoader BLANK_IMAGE_LOADER = new NullImageLoader();
        private static final StringFormatter EMPTY_STRING_FORMATTER = new EmptyStringFormatter();
        private static final Supplier<Integer> CORNER_RADIUS_DEFAULT = Suppliers.of(0);
        private static final Supplier<Boolean> DARK_THEME_DEFAULT = Suppliers.of(false);
        private static final Supplier<Long> FADE_IMAGE_THRESHOLD_DEFAULT =
                Suppliers.of(Long.MAX_VALUE);
        private static final TypefaceProvider NULL_TYPEFACE_PROVIDER = new NullTypefaceProvider();

        private ImageLoader imageLoader = BLANK_IMAGE_LOADER;
        private StringFormatter stringFormatter = EMPTY_STRING_FORMATTER;
        private Supplier<Integer> defaultCornerRadiusSupplier = CORNER_RADIUS_DEFAULT;
        private Supplier<Boolean> isDarkThemeSupplier = DARK_THEME_DEFAULT;
        private Supplier<Long> fadeImageThresholdMsSupplier = FADE_IMAGE_THRESHOLD_DEFAULT;
        private TypefaceProvider typefaceProvider = NULL_TYPEFACE_PROVIDER;
        private Supplier<Boolean> isRtLSupplier = LayoutUtils::isDefaultLocaleRtl;

        private DebugBehavior debugBehavior = DebugBehavior.SILENT;
        /*@MonotonicNonNull*/ private CustomElementProvider customElementProvider;
        /*@MonotonicNonNull*/ private HostBindingProvider hostBindingProvider;
        /*@MonotonicNonNull*/ private Clock clock;
        private boolean allowLegacyRoundedCornerImpl;
        private boolean allowOutlineRoundedCornerImpl;

        private Builder() {}

        public Builder setDebugBehavior(DebugBehavior debugBehavior) {
            this.debugBehavior = debugBehavior;
            return this;
        }

        public Builder setCustomElementProvider(CustomElementProvider customElementProvider) {
            this.customElementProvider = customElementProvider;
            return this;
        }

        public Builder setHostBindingProvider(HostBindingProvider hostBindingProvider) {
            this.hostBindingProvider = hostBindingProvider;
            return this;
        }

        public Builder setClock(Clock clock) {
            this.clock = clock;
            return this;
        }

        /**
         * Use the rounded corner optimizations on JB/KK for better performance at the expense of
         * antialiasing.
         */
        public Builder setAllowLegacyRoundedCornerImpl(boolean allowLegacyRoundedCornerImpl) {
            this.allowLegacyRoundedCornerImpl = allowLegacyRoundedCornerImpl;
            return this;
        }

        /**
         * Use the clipToOutline rounded corner optimizations on L+ when all four corners are
         * rounded for better performance.
         */
        public Builder setAllowOutlineRoundedCornerImpl(boolean allowOutlineRoundedCornerImpl) {
            this.allowOutlineRoundedCornerImpl = allowOutlineRoundedCornerImpl;
            return this;
        }

        // AssetProvider-related setters
        public Builder setImageLoader(ImageLoader imageLoader) {
            this.imageLoader = imageLoader;
            return this;
        }

        public Builder setStringFormatter(StringFormatter stringFormatter) {
            this.stringFormatter = stringFormatter;
            return this;
        }

        public Builder setDefaultCornerRadius(Supplier<Integer> defaultCornerRadiusSupplier) {
            this.defaultCornerRadiusSupplier = defaultCornerRadiusSupplier;
            return this;
        }

        public Builder setFadeImageThresholdMs(Supplier<Long> fadeImageThresholdMsSupplier) {
            this.fadeImageThresholdMsSupplier = fadeImageThresholdMsSupplier;
            return this;
        }

        public Builder setIsDarkTheme(Supplier<Boolean> isDarkThemeSupplier) {
            this.isDarkThemeSupplier = isDarkThemeSupplier;
            return this;
        }

        public Builder setIsRtL(Supplier<Boolean> isRtLSupplier) {
            this.isRtLSupplier = isRtLSupplier;
            return this;
        }

        public Builder setTypefaceProvider(TypefaceProvider typefaceProvider) {
            this.typefaceProvider = typefaceProvider;
            return this;
        }
        // End AssetProvider-related setters

        public PietManager build() {
            customElementProvider = customElementProvider == null
                    ? new ThrowingCustomElementProvider()
                    : customElementProvider;
            hostBindingProvider =
                    hostBindingProvider == null ? new HostBindingProvider() : hostBindingProvider;
            clock = clock == null ? new SystemClockImpl() : clock;

            AssetProvider assetProvider = new AssetProvider(imageLoader, stringFormatter,
                    defaultCornerRadiusSupplier, fadeImageThresholdMsSupplier, isDarkThemeSupplier,
                    isRtLSupplier, typefaceProvider);

            return new PietManagerImpl(debugBehavior, assetProvider, customElementProvider,
                    hostBindingProvider, clock, allowLegacyRoundedCornerImpl,
                    allowOutlineRoundedCornerImpl);
        }
    }
}
