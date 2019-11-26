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
    final AssetProvider mAssetProvider;
    private final DebugBehavior mDebugBehavior;
    private final CustomElementProvider mCustomElementProvider;
    private final HostBindingProvider mHostBindingProvider;
    private final Clock mClock;
    private final boolean mAllowLegacyRoundedCornerImpl;
    private final boolean mAllowOutlineRoundedCornerImpl;
    @VisibleForTesting /*@Nullable*/ AdapterParameters mAdapterParameters;

    PietManagerImpl(DebugBehavior debugBehavior, AssetProvider assetProvider,
            CustomElementProvider customElementProvider, HostBindingProvider hostBindingProvider,
            Clock clock, boolean allowLegacyRoundedCornerImpl,
            boolean allowOutlineRoundedCornerImpl) {
        this.mDebugBehavior = debugBehavior;
        this.mAssetProvider = assetProvider;
        this.mCustomElementProvider = customElementProvider;
        this.mHostBindingProvider = hostBindingProvider;
        this.mClock = clock;
        this.mAllowLegacyRoundedCornerImpl = allowLegacyRoundedCornerImpl;
        this.mAllowOutlineRoundedCornerImpl = allowOutlineRoundedCornerImpl;
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

        return new FrameAdapterImpl(
                context, parameters, actionHandler, eventLogger, mDebugBehavior);
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
        if (mAdapterParameters == null || mAdapterParameters.mContext != context) {
            mAdapterParameters = new AdapterParameters(context, cardViewProducer,
                    new HostProviders(mAssetProvider, mCustomElementProvider, mHostBindingProvider,
                            logDataCallback),
                    mClock, mAllowLegacyRoundedCornerImpl, mAllowOutlineRoundedCornerImpl);
        }
        return mAdapterParameters;
    }

    @Override
    public void purgeRecyclerPools() {
        if (mAdapterParameters != null) {
            AdapterParameters adapterParametersNonNull = mAdapterParameters;
            adapterParametersNonNull.mElementAdapterFactory.purgeRecyclerPools();
            adapterParametersNonNull.mPietStylesHelperFactory.purge();
            adapterParametersNonNull.mRoundedCornerMaskCache.purge();
        }
    }
}
