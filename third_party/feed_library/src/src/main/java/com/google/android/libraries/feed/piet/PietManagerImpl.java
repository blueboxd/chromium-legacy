// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import android.view.ViewGroup;

import com.google.android.libraries.feed.api.host.config.DebugBehavior;
import com.google.android.libraries.feed.common.functional.Supplier;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.piet.host.ActionHandler;
import com.google.android.libraries.feed.piet.host.AssetProvider;
import com.google.android.libraries.feed.piet.host.CustomElementProvider;
import com.google.android.libraries.feed.piet.host.EventLogger;
import com.google.android.libraries.feed.piet.host.HostBindingProvider;
import com.google.android.libraries.feed.piet.host.LogDataCallback;

/** Implementation of {@link PietManager}. */
class PietManagerImpl implements PietManager {
    @VisibleForTesting
    final AssetProvider assetProvider;
    private final DebugBehavior debugBehavior;
    private final CustomElementProvider customElementProvider;
    private final HostBindingProvider hostBindingProvider;
    private final Clock clock;
    private final boolean allowLegacyRoundedCornerImpl;
    private final boolean allowOutlineRoundedCornerImpl;
    @VisibleForTesting /*@Nullable*/ AdapterParameters adapterParameters;

    PietManagerImpl(DebugBehavior debugBehavior, AssetProvider assetProvider,
            CustomElementProvider customElementProvider, HostBindingProvider hostBindingProvider,
            Clock clock, boolean allowLegacyRoundedCornerImpl,
            boolean allowOutlineRoundedCornerImpl) {
        this.debugBehavior = debugBehavior;
        this.assetProvider = assetProvider;
        this.customElementProvider = customElementProvider;
        this.hostBindingProvider = hostBindingProvider;
        this.clock = clock;
        this.allowLegacyRoundedCornerImpl = allowLegacyRoundedCornerImpl;
        this.allowOutlineRoundedCornerImpl = allowOutlineRoundedCornerImpl;
    }

    @Override
    public FrameAdapter createPietFrameAdapter(Supplier</*@Nullable*/ ViewGroup> cardViewProducer,
            ActionHandler actionHandler, EventLogger eventLogger, Context context) {
        return createPietFrameAdapter(cardViewProducer, actionHandler, eventLogger, context, null);
    }

    @Override
    public FrameAdapter createPietFrameAdapter(Supplier</*@Nullable*/ ViewGroup> cardViewProducer,
            ActionHandler actionHandler, EventLogger eventLogger, Context context,
            /*@Nullable*/ LogDataCallback logDataCallback) {
        AdapterParameters parameters =
                getAdapterParameters(context, cardViewProducer, logDataCallback);

        return new FrameAdapterImpl(context, parameters, actionHandler, eventLogger, debugBehavior);
    }

    /**
     * Return the {@link AdapterParameters}. If one doesn't exist this will create a new instance.
     * The
     * {@code AdapterParameters} is scoped to the {@code Context}.
     */
    @VisibleForTesting
    AdapterParameters getAdapterParameters(Context context,
            Supplier</*@Nullable*/ ViewGroup> cardViewProducer,
            /*@Nullable*/ LogDataCallback logDataCallback) {
        if (adapterParameters == null || adapterParameters.context != context) {
            adapterParameters = new AdapterParameters(context, cardViewProducer,
                    new HostProviders(assetProvider, customElementProvider, hostBindingProvider,
                            logDataCallback),
                    clock, allowLegacyRoundedCornerImpl, allowOutlineRoundedCornerImpl);
        }
        return adapterParameters;
    }

    @Override
    public void purgeRecyclerPools() {
        if (adapterParameters != null) {
            AdapterParameters adapterParametersNonNull = adapterParameters;
            adapterParametersNonNull.elementAdapterFactory.purgeRecyclerPools();
            adapterParametersNonNull.pietStylesHelperFactory.purge();
            adapterParametersNonNull.roundedCornerMaskCache.purge();
        }
    }
}
