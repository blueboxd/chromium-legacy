// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.drivers;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkNotNull;
import static org.chromium.chrome.browser.feed.library.common.Validators.checkState;

import android.content.Context;
import android.view.View;
import android.view.View.OnClickListener;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.InternalFeedError;
import org.chromium.chrome.browser.feed.library.api.host.logging.SpinnerType;
import org.chromium.chrome.browser.feed.library.api.host.stream.SnackbarApi;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild.Type;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelCursor;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelError;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelToken;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.TokenCompleted;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.TokenCompletedObserver;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.ContinuationViewHolder;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.FeedViewHolder;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.ViewHolderType;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.sharedstream.logging.LoggingListener;
import org.chromium.chrome.browser.feed.library.sharedstream.logging.SpinnerLogger;

import java.util.ArrayList;
import java.util.List;

/** {@link FeatureDriver} for the more button. */
public class ContinuationDriver extends LeafFeatureDriver
        implements OnClickListener, LoggingListener, TokenCompletedObserver {
    private static final String TAG = "ContinuationDriver";
    private final BasicLoggingApi mBasicLoggingApi;
    private final Context mContext;
    private final CursorChangedListener mCursorChangedListener;
    private final ModelChild mModelChild;
    private final ModelToken mModelToken;
    private final ModelProvider mModelProvider;
    private final int mPosition;
    private final SnackbarApi mSnackbarApi;
    private final SpinnerLogger mSpinnerLogger;
    private final ThreadUtils mThreadUtils;
    private final boolean mConsumeSyntheticTokens;

    private boolean mShowSpinner;
    private boolean mInitialized;
    private boolean mViewLogged;
    private boolean mDestroyed;
    private boolean mTokenHandled;
    private int mFailureCount;
    @SpinnerType
    private int mSpinnerType = SpinnerType.INFINITE_FEED;
    @Nullable
    private ContinuationViewHolder mContinuationViewHolder;

    ContinuationDriver(BasicLoggingApi basicLoggingApi, Clock clock, Configuration configuration,
            Context context, CursorChangedListener cursorChangedListener, ModelChild modelChild,
            ModelProvider modelProvider, int position, SnackbarApi snackbarApi,
            ThreadUtils threadUtils, boolean restoring) {
        this.mBasicLoggingApi = basicLoggingApi;
        this.mContext = context;
        this.mCursorChangedListener = cursorChangedListener;
        this.mModelChild = modelChild;
        this.mModelProvider = modelProvider;
        this.mModelToken = modelChild.getModelToken();
        this.mPosition = position;
        this.mSnackbarApi = snackbarApi;
        this.mSpinnerLogger = createSpinnerLogger(basicLoggingApi, clock);
        this.mThreadUtils = threadUtils;
        this.mShowSpinner =
                configuration.getValueOrDefault(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, false);
        this.mConsumeSyntheticTokens =
                configuration.getValueOrDefault(ConfigKey.CONSUME_SYNTHETIC_TOKENS, false)
                || (restoring
                        && configuration.getValueOrDefault(
                                ConfigKey.CONSUME_SYNTHETIC_TOKENS_WHILE_RESTORING, false));
    }

    public boolean hasTokenBeenHandled() {
        return mTokenHandled;
    }

    public void initialize() {
        if (mInitialized) {
            return;
        }

        mInitialized = true;
        mModelToken.registerObserver(this);
        if (mModelToken.isSynthetic() && mConsumeSyntheticTokens) {
            Logger.d(TAG, "Handling synthetic token");
            boolean tokenWillBeHandled = mModelProvider.handleToken(mModelToken);
            if (tokenWillBeHandled) {
                mShowSpinner = true;
                mSpinnerType = SpinnerType.SYNTHETIC_TOKEN;
                mTokenHandled = true;
            } else {
                Logger.e(TAG, "Synthetic token was not handled");
                mBasicLoggingApi.onInternalError(InternalFeedError.UNHANDLED_TOKEN);
            }
        }
    }

    @Override
    public void onDestroy() {
        mDestroyed = true;

        if (mInitialized) {
            mModelToken.unregisterObserver(this);
        }
        // If the spinner was being shown, it will only be removed when the ContinuationDriver is
        // destroyed. So onSpinnerShown should be logged then.
        if (mSpinnerLogger.isSpinnerActive()) {
            mSpinnerLogger.spinnerDestroyedWithoutCompleting();
        }
    }

    // TODO: Instead of implementing an onClickListener, define a new interface with a method
    // with no view argument.
    @Override
    public void onClick(View v) {
        if (!isBound()) {
            Logger.wtf(TAG, "Calling onClick before binding.");
            return;
        }
        boolean tokenWillBeHandled = mModelProvider.handleToken(mModelToken);
        if (tokenWillBeHandled) {
            mShowSpinner = true;
            mSpinnerLogger.spinnerStarted(SpinnerType.MORE_BUTTON);
            checkNotNull(mContinuationViewHolder).setShowSpinner(true);
            mTokenHandled = true;
        } else {
            Logger.e(TAG, "Continuation token was not handled");
            mBasicLoggingApi.onInternalError(InternalFeedError.UNHANDLED_TOKEN);
            showErrorUi();
        }
    }

    @Override
    public void bind(FeedViewHolder viewHolder) {
        checkState(mInitialized);
        if (isBound()) {
            Logger.wtf(TAG, "Rebinding.");
        }
        checkState(viewHolder instanceof ContinuationViewHolder);

        mContinuationViewHolder = (ContinuationViewHolder) viewHolder;
        mContinuationViewHolder.bind(
                /* onClickListener= */ this, /* loggingListener= */ this, mShowSpinner);
        if (mShowSpinner && !mSpinnerLogger.isSpinnerActive()) {
            mSpinnerLogger.spinnerStarted(mSpinnerType);
        }
    }

    @Override
    public int getItemViewType() {
        return ViewHolderType.TYPE_CONTINUATION;
    }

    @Override
    public void unbind() {
        if (mContinuationViewHolder == null) {
            return;
        }

        mContinuationViewHolder.unbind();
        mContinuationViewHolder = null;
    }

    @Override
    public void maybeRebind() {
        if (mContinuationViewHolder == null) {
            return;
        }

        // Unbinding clears the viewHolder, so storing to rebind.
        ContinuationViewHolder localViewHolder = mContinuationViewHolder;
        unbind();
        bind(localViewHolder);
    }

    @Override
    public void onTokenCompleted(TokenCompleted tokenCompleted) {
        mThreadUtils.checkMainThread();
        if (mDestroyed) {
            // Tokens are able to send onTokenCompleted even after unregistering.  This can happen
            // due to thread switching.  This prevents tokens from being handled after the driver
            // has been destroyed and should no longer be handled.
            Logger.w(TAG, "Received onTokenCompleted after being destroyed.");
            return;
        }

        // Spinner wouldn't be active if we are automatically consuming a synthetic token on
        // restore.
        if (mSpinnerLogger.isSpinnerActive()) {
            mSpinnerLogger.spinnerFinished();
        }

        ModelCursor cursor = tokenCompleted.getCursor();
        List<ModelChild> modelChildren = extractModelChildrenFromCursor(cursor);

        // Display snackbar if there are no more cards. The snackbar should only be shown if the
        // spinner is being shown. This ensures the snackbar is only shown in the instance of the
        // Stream that triggered the pagination.
        if (mShowSpinner
                && (modelChildren.isEmpty()
                        || (modelChildren.size() == 1
                                && modelChildren.get(0).getType() == Type.TOKEN))) {
            mSnackbarApi.show(mContext.getResources().getString(
                    R.string.ntp_suggestions_fetch_no_new_suggestions));
        }

        mCursorChangedListener.onNewChildren(mModelChild, modelChildren, mModelToken.isSynthetic());
    }

    @Override
    public void onError(ModelError modelError) {
        mBasicLoggingApi.onTokenFailedToComplete(mModelToken.isSynthetic(), ++mFailureCount);
        showErrorUi();
        if (mSpinnerLogger.isSpinnerActive()) {
            mSpinnerLogger.spinnerFinished();
        }
    }

    private void showErrorUi() {
        mShowSpinner = false;

        if (mContinuationViewHolder != null) {
            mContinuationViewHolder.setShowSpinner(false);
        }

        mSnackbarApi.show(mContext.getString(R.string.ntp_suggestions_fetch_failed));
    }

    @Override
    public void onViewVisible() {
        // Do not log a view if the spinner is being shown.
        if (mViewLogged || mShowSpinner) {
            return;
        }

        mBasicLoggingApi.onMoreButtonViewed(mPosition);
        mViewLogged = true;
    }

    @Override
    public void onContentClicked() {
        mBasicLoggingApi.onMoreButtonClicked(mPosition);
    }

    @Override
    public void onContentSwiped() {}

    // TODO: add similar view logging cancelation and time delay for the More button
    @Override
    public void onScrollStateChanged(int newScrollState) {}

    private List<ModelChild> extractModelChildrenFromCursor(ModelCursor cursor) {
        List<ModelChild> modelChildren = new ArrayList<>();
        ModelChild child;
        while ((child = cursor.getNextItem()) != null) {
            if (child.getType() == Type.UNBOUND) {
                Logger.e(TAG, "Found unbound child %s, ignoring it", child.getContentId());
                continue;
            } else if (child.getType() != Type.FEATURE && child.getType() != Type.TOKEN) {
                Logger.wtf(TAG, "Received illegal child: %s from cursor.", child.getType());
                continue;
            }
            modelChildren.add(child);
        }

        return modelChildren;
    }

    /** Interface for notifying parents of new model children from a token. */
    public interface CursorChangedListener {
        /**
         * Called to inform parent of new model children.
         *
         * @param modelChild the {@link ModelChild} representing the token that was processed.
         * @param modelChildren the list of new {@link ModelChild} from the token.
         * @param wasSynthetic whether the token was synthetic.
         */
        void onNewChildren(
                ModelChild modelChild, List<ModelChild> modelChildren, boolean wasSynthetic);
    }

    @VisibleForTesting
    boolean isBound() {
        return mContinuationViewHolder != null;
    }

    @VisibleForTesting
    SpinnerLogger createSpinnerLogger(BasicLoggingApi basicLoggingApi, Clock clock) {
        return new SpinnerLogger(basicLoggingApi, clock);
    }

    @Override
    public String getContentId() {
        return mModelToken.getStreamToken().getContentId();
    }
}
