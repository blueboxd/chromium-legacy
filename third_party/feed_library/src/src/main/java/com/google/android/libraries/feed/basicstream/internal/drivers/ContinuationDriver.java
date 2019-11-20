// Copyright 2018 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.android.libraries.feed.basicstream.internal.drivers;

import static com.google.android.libraries.feed.common.Validators.checkNotNull;
import static com.google.android.libraries.feed.common.Validators.checkState;

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import android.view.View;
import android.view.View.OnClickListener;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.InternalFeedError;
import com.google.android.libraries.feed.api.host.logging.SpinnerType;
import com.google.android.libraries.feed.api.host.stream.SnackbarApi;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild.Type;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelCursor;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelError;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelToken;
import com.google.android.libraries.feed.api.internal.modelprovider.TokenCompleted;
import com.google.android.libraries.feed.api.internal.modelprovider.TokenCompletedObserver;
import com.google.android.libraries.feed.basicstream.internal.viewholders.ContinuationViewHolder;
import com.google.android.libraries.feed.basicstream.internal.viewholders.FeedViewHolder;
import com.google.android.libraries.feed.basicstream.internal.viewholders.R;
import com.google.android.libraries.feed.basicstream.internal.viewholders.ViewHolderType;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.sharedstream.logging.LoggingListener;
import com.google.android.libraries.feed.sharedstream.logging.SpinnerLogger;
import java.util.ArrayList;
import java.util.List;

/** {@link FeatureDriver} for the more button. */
public class ContinuationDriver extends LeafFeatureDriver
    implements OnClickListener, LoggingListener, TokenCompletedObserver {

  private static final String TAG = "ContinuationDriver";
  private final BasicLoggingApi basicLoggingApi;
  private final Context context;
  private final CursorChangedListener cursorChangedListener;
  private final ModelChild modelChild;
  private final ModelToken modelToken;
  private final ModelProvider modelProvider;
  private final int position;
  private final SnackbarApi snackbarApi;
  private final SpinnerLogger spinnerLogger;
  private final ThreadUtils threadUtils;
  private final boolean consumeSyntheticTokens;

  private boolean showSpinner;
  private boolean initialized;
  private boolean viewLogged;
  private boolean destroyed;
  private boolean tokenHandled;
  private int failureCount = 0;
  @SpinnerType private int spinnerType = SpinnerType.INFINITE_FEED;
  /*@Nullable*/ private ContinuationViewHolder continuationViewHolder;

  ContinuationDriver(
      BasicLoggingApi basicLoggingApi,
      Clock clock,
      Configuration configuration,
      Context context,
      CursorChangedListener cursorChangedListener,
      ModelChild modelChild,
      ModelProvider modelProvider,
      int position,
      SnackbarApi snackbarApi,
      ThreadUtils threadUtils,
      boolean restoring) {
    this.basicLoggingApi = basicLoggingApi;
    this.context = context;
    this.cursorChangedListener = cursorChangedListener;
    this.modelChild = modelChild;
    this.modelProvider = modelProvider;
    this.modelToken = modelChild.getModelToken();
    this.position = position;
    this.snackbarApi = snackbarApi;
    this.spinnerLogger = createSpinnerLogger(basicLoggingApi, clock);
    this.threadUtils = threadUtils;
    this.showSpinner =
        configuration.getValueOrDefault(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, false);
    this.consumeSyntheticTokens =
        configuration.getValueOrDefault(ConfigKey.CONSUME_SYNTHETIC_TOKENS, false)
            || (restoring
                && configuration.getValueOrDefault(
                    ConfigKey.CONSUME_SYNTHETIC_TOKENS_WHILE_RESTORING, false));
  }

  public boolean hasTokenBeenHandled() {
    return tokenHandled;
  }

  public void initialize() {
    if (initialized) {
      return;
    }

    initialized = true;
    modelToken.registerObserver(this);
    if (modelToken.isSynthetic() && consumeSyntheticTokens) {
      Logger.d(TAG, "Handling synthetic token");
      boolean tokenWillBeHandled = modelProvider.handleToken(modelToken);
      if (tokenWillBeHandled) {
        showSpinner = true;
        spinnerType = SpinnerType.SYNTHETIC_TOKEN;
        tokenHandled = true;
      } else {
        Logger.e(TAG, "Synthetic token was not handled");
        basicLoggingApi.onInternalError(InternalFeedError.UNHANDLED_TOKEN);
      }
    }
  }

  @Override
  public void onDestroy() {
    destroyed = true;

    if (initialized) {
      modelToken.unregisterObserver(this);
    }
    // If the spinner was being shown, it will only be removed when the ContinuationDriver is
    // destroyed. So onSpinnerShown should be logged then.
    if (spinnerLogger.isSpinnerActive()) {
      spinnerLogger.spinnerDestroyedWithoutCompleting();
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
    boolean tokenWillBeHandled = modelProvider.handleToken(modelToken);
    if (tokenWillBeHandled) {
      showSpinner = true;
      spinnerLogger.spinnerStarted(SpinnerType.MORE_BUTTON);
      checkNotNull(continuationViewHolder).setShowSpinner(true);
      tokenHandled = true;
    } else {
      Logger.e(TAG, "Continuation token was not handled");
      basicLoggingApi.onInternalError(InternalFeedError.UNHANDLED_TOKEN);
      showErrorUi();
    }
  }

  @Override
  public void bind(FeedViewHolder viewHolder) {
    checkState(initialized);
    if (isBound()) {
      Logger.wtf(TAG, "Rebinding.");
    }
    checkState(viewHolder instanceof ContinuationViewHolder);

    continuationViewHolder = (ContinuationViewHolder) viewHolder;
    continuationViewHolder.bind(
        /* onClickListener= */ this, /* loggingListener= */ this, showSpinner);
    if (showSpinner && !spinnerLogger.isSpinnerActive()) {
      spinnerLogger.spinnerStarted(spinnerType);
    }
  }

  @Override
  public int getItemViewType() {
    return ViewHolderType.TYPE_CONTINUATION;
  }

  @Override
  public void unbind() {
    if (continuationViewHolder == null) {
      return;
    }

    continuationViewHolder.unbind();
    continuationViewHolder = null;
  }

  @Override
  public void maybeRebind() {
    if (continuationViewHolder == null) {
      return;
    }

    // Unbinding clears the viewHolder, so storing to rebind.
    ContinuationViewHolder localViewHolder = continuationViewHolder;
    unbind();
    bind(localViewHolder);
  }

  @Override
  public void onTokenCompleted(TokenCompleted tokenCompleted) {
    threadUtils.checkMainThread();
    if (destroyed) {
      // Tokens are able to send onTokenCompleted even after unregistering.  This can happen due to
      // thread switching.  This prevents tokens from being handled after the driver has been
      // destroyed and should no longer be handled.
      Logger.w(TAG, "Received onTokenCompleted after being destroyed.");
      return;
    }

    // Spinner wouldn't be active if we are automatically consuming a synthetic token on restore.
    if (spinnerLogger.isSpinnerActive()) {
      spinnerLogger.spinnerFinished();
    }

    ModelCursor cursor = tokenCompleted.getCursor();
    List<ModelChild> modelChildren = extractModelChildrenFromCursor(cursor);

    // Display snackbar if there are no more cards. The snackbar should only be shown if the
    // spinner is being shown. This ensures the snackbar is only shown in the instance of the Stream
    // that triggered the pagination.
    if (showSpinner
        && (modelChildren.isEmpty()
            || (modelChildren.size() == 1 && modelChildren.get(0).getType() == Type.TOKEN))) {
      snackbarApi.show(
          context.getResources().getString(R.string.snackbar_fetch_no_new_suggestions));
    }

    cursorChangedListener.onNewChildren(modelChild, modelChildren, modelToken.isSynthetic());
  }

  @Override
  public void onError(ModelError modelError) {
    basicLoggingApi.onTokenFailedToComplete(modelToken.isSynthetic(), ++failureCount);
    showErrorUi();
    spinnerLogger.spinnerFinished();
  }

  private void showErrorUi() {
    showSpinner = false;

    if (continuationViewHolder != null) {
      continuationViewHolder.setShowSpinner(false);
    }

    snackbarApi.show(context.getString(R.string.snackbar_fetch_failed));
  }

  @Override
  public void onViewVisible() {
    // Do not log a view if the spinner is being shown.
    if (viewLogged || showSpinner) {
      return;
    }

    basicLoggingApi.onMoreButtonViewed(position);
    viewLogged = true;
  }

  @Override
  public void onContentClicked() {
    basicLoggingApi.onMoreButtonClicked(position);
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
    void onNewChildren(ModelChild modelChild, List<ModelChild> modelChildren, boolean wasSynthetic);
  }

  @VisibleForTesting
  boolean isBound() {
    return continuationViewHolder != null;
  }

  @VisibleForTesting
  SpinnerLogger createSpinnerLogger(
      /*@UnderInitialization*/ ContinuationDriver this, BasicLoggingApi basicLoggingApi, Clock clock) {
    return new SpinnerLogger(basicLoggingApi, clock);
  }

  @Override
  public String getContentId() {
    return modelToken.getStreamToken().getContentId();
  }
}
