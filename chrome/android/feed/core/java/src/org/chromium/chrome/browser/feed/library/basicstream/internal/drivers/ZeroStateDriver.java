// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.drivers;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkState;

import android.support.annotation.VisibleForTesting;
import android.view.View;
import android.view.View.OnClickListener;

import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ContentChangedListener;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.RequestReason;
import org.chromium.chrome.browser.feed.library.api.host.logging.SpinnerType;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.FeedViewHolder;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.ViewHolderType;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.ZeroStateViewHolder;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.sharedstream.logging.SpinnerLogger;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;
import org.chromium.components.feed.core.proto.libraries.sharedstream.UiRefreshReasonProto.UiRefreshReason;
import org.chromium.components.feed.core.proto.libraries.sharedstream.UiRefreshReasonProto.UiRefreshReason.Reason;

/** {@link FeatureDriver} for the zero state. */
public class ZeroStateDriver extends LeafFeatureDriver implements OnClickListener {
    private static final String TAG = "ZeroStateDriver";

    private final ContentChangedListener mContentChangedListener;
    private final SpinnerLogger mSpinnerLogger;

    private ModelProvider mModelProvider;
    private boolean mSpinnerShown;
    /*@Nullable*/ private ZeroStateViewHolder mZeroStateViewHolder;

    ZeroStateDriver(BasicLoggingApi basicLoggingApi, Clock clock, ModelProvider modelProvider,
            ContentChangedListener contentChangedListener, boolean spinnerShown) {
        this.mContentChangedListener = contentChangedListener;
        this.mModelProvider = modelProvider;
        this.mSpinnerLogger = createSpinnerLogger(basicLoggingApi, clock);
        this.mSpinnerShown = spinnerShown;
    }

    @Override
    public void bind(FeedViewHolder viewHolder) {
        if (isBound()) {
            Logger.w(TAG, "Rebinding.");
            if (viewHolder == mZeroStateViewHolder) {
                Logger.e(TAG, "Being rebound to the previously bound viewholder");
                return;
            }
            unbind();
        }
        checkState(viewHolder instanceof ZeroStateViewHolder);

        mZeroStateViewHolder = (ZeroStateViewHolder) viewHolder;
        mZeroStateViewHolder.bind(this, mSpinnerShown);
        // Only log that spinner is being shown if it has not been logged before.
        if (mSpinnerShown && !mSpinnerLogger.isSpinnerActive()) {
            mSpinnerLogger.spinnerStarted(SpinnerType.INITIAL_LOAD);
        }
    }

    @Override
    public int getItemViewType() {
        return ViewHolderType.TYPE_ZERO_STATE;
    }

    @Override
    public void unbind() {
        if (mZeroStateViewHolder == null) {
            return;
        }

        mZeroStateViewHolder.unbind();
        mZeroStateViewHolder = null;
    }

    @Override
    public void maybeRebind() {
        if (mZeroStateViewHolder == null) {
            return;
        }

        // Unbinding clears the viewHolder, so storing to rebind.
        ZeroStateViewHolder localViewHolder = mZeroStateViewHolder;
        unbind();
        bind(localViewHolder);
    }

    @Override
    public void onClick(View v) {
        if (mZeroStateViewHolder == null) {
            Logger.wtf(TAG, "Calling onClick before binding.");
            return;
        }

        mSpinnerShown = true;
        mZeroStateViewHolder.showSpinner(mSpinnerShown);
        mContentChangedListener.onContentChanged();
        UiContext uiContext =
                UiContext.newBuilder()
                        .setExtension(UiRefreshReason.uiRefreshReasonExtension,
                                UiRefreshReason.newBuilder().setReason(Reason.ZERO_STATE).build())
                        .build();
        mModelProvider.triggerRefresh(RequestReason.ZERO_STATE, uiContext);

        mSpinnerLogger.spinnerStarted(SpinnerType.ZERO_STATE_REFRESH);
    }

    @Override
    public void onDestroy() {
        // If the spinner was being shown, it will only be removed when the ZeroStateDriver is
        // destroyed. So spinner should be logged then.
        if (mSpinnerLogger.isSpinnerActive()) {
            mSpinnerLogger.spinnerFinished();
        }
    }

    /**
     * Updates the model provider.
     *
     * <p>This is a hacky way to keep the model providers in sync in the situation where switching
     * stream drivers would result in the zero state flashing.
     */
    void setModelProvider(ModelProvider modelProvider) {
        this.mModelProvider = modelProvider;
    }

    @VisibleForTesting
    boolean isBound() {
        return mZeroStateViewHolder != null;
    }

    @VisibleForTesting
    SpinnerLogger createSpinnerLogger(
            /*@UnderInitialization*/ ZeroStateDriver this, BasicLoggingApi basicLoggingApi,
            Clock clock) {
        return new SpinnerLogger(basicLoggingApi, clock);
    }

    /** Returns whether the spinner is showing. */
    boolean isSpinnerShowing() {
        return mSpinnerShown;
    }
}
