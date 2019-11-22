// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import android.view.View;
import android.view.ViewGroup;

import com.google.android.libraries.feed.common.functional.Supplier;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.piet.PietStylesHelper.PietStylesHelperFactory;
import com.google.android.libraries.feed.piet.ui.RoundedCornerMaskCache;

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

    final Context context;
    final Supplier</*@Nullable*/ ViewGroup> parentViewSupplier;
    final HostProviders hostProviders;
    final ParameterizedTextEvaluator templatedStringEvaluator;
    final ElementAdapterFactory elementAdapterFactory;
    final TemplateBinder templateBinder;
    final StyleProvider defaultStyleProvider;
    final Clock clock;
    final PietStylesHelperFactory pietStylesHelperFactory;
    final RoundedCornerMaskCache roundedCornerMaskCache;
    final boolean allowLegacyRoundedCornerImpl;
    final boolean allowOutlineRoundedCornerImpl;

    // Doesn't like passing "this" to the new ElementAdapterFactory; however, nothing in the
    // factory's construction will reference the elementAdapterFactory member of this, so should be
    // safe.
    @SuppressWarnings("initialization")
    public AdapterParameters(Context context, Supplier</*@Nullable*/ ViewGroup> parentViewSupplier,
            HostProviders hostProviders, Clock clock, boolean allowLegacyRoundedCornerImpl,
            boolean allowOutlineRoundedCornerImpl) {
        this.context = context;
        this.parentViewSupplier = parentViewSupplier;
        this.hostProviders = hostProviders;
        this.clock = clock;

        templatedStringEvaluator = new ParameterizedTextEvaluator(clock);

        KeyedRecyclerPool<ElementAdapter<? extends View, ?>> templateRecyclerPool =
                new KeyedRecyclerPool<>(DEFAULT_NUM_TEMPLATE_POOLS, DEFAULT_TEMPLATE_POOL_SIZE);
        elementAdapterFactory = new ElementAdapterFactory(context, this, templateRecyclerPool);
        templateBinder = new TemplateBinder(templateRecyclerPool, elementAdapterFactory);

        this.defaultStyleProvider = new StyleProvider(hostProviders.getAssetProvider());

        this.pietStylesHelperFactory = new PietStylesHelperFactory();
        this.roundedCornerMaskCache = new RoundedCornerMaskCache();

        this.allowLegacyRoundedCornerImpl = allowLegacyRoundedCornerImpl;
        this.allowOutlineRoundedCornerImpl = allowOutlineRoundedCornerImpl;
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
        this.context = context;
        this.parentViewSupplier = parentViewSupplier;
        this.hostProviders = hostProviders;

        this.templatedStringEvaluator = templatedStringEvaluator;
        this.elementAdapterFactory = elementAdapterFactory;
        this.templateBinder = templateBinder;
        this.defaultStyleProvider = new StyleProvider(hostProviders.getAssetProvider());
        this.clock = clock;
        this.pietStylesHelperFactory = pietStylesHelperFactory;
        this.roundedCornerMaskCache = maskCache;
        this.allowLegacyRoundedCornerImpl = allowLegacyRoundedCornerImpl;
        this.allowOutlineRoundedCornerImpl = allowOutlineRoundedCornerImpl;
    }
}
