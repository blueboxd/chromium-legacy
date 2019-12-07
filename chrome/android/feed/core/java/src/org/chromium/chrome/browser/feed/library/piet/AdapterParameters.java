// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.Supplier;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.piet.PietStylesHelper.PietStylesHelperFactory;
import org.chromium.chrome.browser.feed.library.piet.ui.RoundedCornerMaskCache;

/**
 * A state shared by instances of Cards and Slices. The state is accessed directly from the instance
 * instead of going through getX methods.
 *
 * <p>This is basically the Dagger state for Piet. If we can use Dagger, replace this with Dagger.
 */
class AdapterParameters {
    // TODO: Make these configurable instead of constants.
    private static final int DEFAULT_TEMPLATE_POOL_SIZE = 100;
    private static final int DEFAULT_NUM_TEMPLATE_POOLS = 30;

    final Context mContext;
    final Supplier</*@Nullable*/ ViewGroup> mParentViewSupplier;
    final HostProviders mHostProviders;
    final ParameterizedTextEvaluator mTemplatedStringEvaluator;
    final ElementAdapterFactory mElementAdapterFactory;
    final TemplateBinder mTemplateBinder;
    final StyleProvider mDefaultStyleProvider;
    final Clock mClock;
    final PietStylesHelperFactory mPietStylesHelperFactory;
    final RoundedCornerMaskCache mRoundedCornerMaskCache;
    final boolean mAllowLegacyRoundedCornerImpl;
    final boolean mAllowOutlineRoundedCornerImpl;

    // Doesn't like passing "this" to the new ElementAdapterFactory; however, nothing in the
    // factory's construction will reference the elementAdapterFactory member of this, so should be
    // safe.
    @SuppressWarnings("initialization")
    public AdapterParameters(Context context, Supplier</*@Nullable*/ ViewGroup> parentViewSupplier,
            HostProviders hostProviders, Clock clock, boolean allowLegacyRoundedCornerImpl,
            boolean allowOutlineRoundedCornerImpl) {
        this.mContext = context;
        this.mParentViewSupplier = parentViewSupplier;
        this.mHostProviders = hostProviders;
        this.mClock = clock;

        mTemplatedStringEvaluator = new ParameterizedTextEvaluator(clock);

        KeyedRecyclerPool<ElementAdapter<? extends View, ?>> templateRecyclerPool =
                new KeyedRecyclerPool<>(DEFAULT_NUM_TEMPLATE_POOLS, DEFAULT_TEMPLATE_POOL_SIZE);
        mElementAdapterFactory = new ElementAdapterFactory(context, this, templateRecyclerPool);
        mTemplateBinder = new TemplateBinder(templateRecyclerPool, mElementAdapterFactory);

        this.mDefaultStyleProvider = new StyleProvider(hostProviders.getAssetProvider());

        this.mPietStylesHelperFactory = new PietStylesHelperFactory();
        this.mRoundedCornerMaskCache = new RoundedCornerMaskCache();

        this.mAllowLegacyRoundedCornerImpl = allowLegacyRoundedCornerImpl;
        this.mAllowOutlineRoundedCornerImpl = allowOutlineRoundedCornerImpl;
    }

    /** Testing-only constructor for mocking the internally-constructed objects. */
    @VisibleForTesting
    AdapterParameters(Context context, Supplier</*@Nullable*/ ViewGroup> parentViewSupplier,
            HostProviders hostProviders, ParameterizedTextEvaluator templatedStringEvaluator,
            ElementAdapterFactory elementAdapterFactory, TemplateBinder templateBinder,
            Clock clock) {
        this(context, parentViewSupplier, hostProviders, templatedStringEvaluator,
                elementAdapterFactory, templateBinder, clock, new PietStylesHelperFactory(),
                new RoundedCornerMaskCache(), true, true);
    }

    /** Testing-only constructor for mocking the internally-constructed objects. */
    @VisibleForTesting
    AdapterParameters(Context context, Supplier</*@Nullable*/ ViewGroup> parentViewSupplier,
            HostProviders hostProviders, ParameterizedTextEvaluator templatedStringEvaluator,
            ElementAdapterFactory elementAdapterFactory, TemplateBinder templateBinder, Clock clock,
            PietStylesHelperFactory pietStylesHelperFactory, RoundedCornerMaskCache maskCache,
            boolean allowLegacyRoundedCornerImpl, boolean allowOutlineRoundedCornerImpl) {
        this.mContext = context;
        this.mParentViewSupplier = parentViewSupplier;
        this.mHostProviders = hostProviders;

        this.mTemplatedStringEvaluator = templatedStringEvaluator;
        this.mElementAdapterFactory = elementAdapterFactory;
        this.mTemplateBinder = templateBinder;
        this.mDefaultStyleProvider = new StyleProvider(hostProviders.getAssetProvider());
        this.mClock = clock;
        this.mPietStylesHelperFactory = pietStylesHelperFactory;
        this.mRoundedCornerMaskCache = maskCache;
        this.mAllowLegacyRoundedCornerImpl = allowLegacyRoundedCornerImpl;
        this.mAllowOutlineRoundedCornerImpl = allowOutlineRoundedCornerImpl;
    }
}
