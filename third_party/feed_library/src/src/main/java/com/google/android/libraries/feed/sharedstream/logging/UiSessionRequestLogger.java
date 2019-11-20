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

package com.google.android.libraries.feed.sharedstream.logging;

import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.SessionEvent;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelError;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderObserver;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.search.now.feed.client.StreamDataProto.UiContext;

/** Logs the initial event after sessions have been requested by the UI. */
public class UiSessionRequestLogger implements ModelProviderObserver {

  private static final String TAG = "UiSessionRequestLogger";
  private final Clock clock;
  private final BasicLoggingApi basicLoggingApi;
  private int sessionCount = 0;
  /*@Nullable*/ private SessionRequestState sessionRequestState;

  public UiSessionRequestLogger(Clock clock, BasicLoggingApi basicLoggingApi) {
    this.clock = clock;
    this.basicLoggingApi = basicLoggingApi;
  }

  /** Should be called whenever the UI requests a new session from the {@link ModelProvider}. */
  public void onSessionRequested(ModelProvider modelProvider) {
    if (sessionRequestState != null) {
      sessionRequestState.modelProvider.unregisterObserver(this);
    }

    this.sessionRequestState = new SessionRequestState(modelProvider, clock);
    modelProvider.registerObserver(this);
  }

  @Override
  public void onSessionStart(UiContext uiContext) {
    if (sessionRequestState == null) {
      Logger.wtf(TAG, "onSessionStart() called without SessionRequestState.");
      return;
    }

    SessionRequestState localSessionRequestState = sessionRequestState;

    basicLoggingApi.onInitialSessionEvent(
        SessionEvent.STARTED, localSessionRequestState.getElapsedTime(), ++sessionCount);

    localSessionRequestState.getModelProvider().unregisterObserver(this);
    sessionRequestState = null;
  }

  @Override
  public void onSessionFinished(UiContext uiContext) {
    if (sessionRequestState == null) {
      Logger.wtf(TAG, "onSessionFinished() called without SessionRequestState.");
      return;
    }

    SessionRequestState localSessionRequestState = sessionRequestState;

    basicLoggingApi.onInitialSessionEvent(
        SessionEvent.FINISHED_IMMEDIATELY,
        localSessionRequestState.getElapsedTime(),
        ++sessionCount);

    localSessionRequestState.getModelProvider().unregisterObserver(this);
    sessionRequestState = null;
  }

  @Override
  public void onError(ModelError modelError) {
    if (sessionRequestState == null) {
      Logger.wtf(TAG, "onError() called without SessionRequestState.");
      return;
    }

    SessionRequestState localSessionRequestState = sessionRequestState;

    basicLoggingApi.onInitialSessionEvent(
        SessionEvent.ERROR, localSessionRequestState.getElapsedTime(), ++sessionCount);

    localSessionRequestState.getModelProvider().unregisterObserver(this);
    sessionRequestState = null;
  }

  /** Should be called whenever the UI is destroyed, and will log the event if necessary. */
  public void onDestroy() {
    if (sessionRequestState == null) {
      // We don't wtf here as to allow onDestroy() to be called regardless of the internal state.
      return;
    }

    SessionRequestState localSessionRequestState = sessionRequestState;

    basicLoggingApi.onInitialSessionEvent(
        SessionEvent.USER_ABANDONED, localSessionRequestState.getElapsedTime(), ++sessionCount);

    localSessionRequestState.getModelProvider().unregisterObserver(this);
    sessionRequestState = null;
  }

  /** Encapsulates the state of whether a session has been requested and when it was requested. */
  private static class SessionRequestState {
    private final long sessionRequestTime;
    private final ModelProvider modelProvider;
    private final Clock clock;

    private SessionRequestState(ModelProvider modelProvider, Clock clock) {
      this.sessionRequestTime = clock.elapsedRealtime();
      this.modelProvider = modelProvider;
      this.clock = clock;
    }

    int getElapsedTime() {
      return (int) (clock.elapsedRealtime() - sessionRequestTime);
    }

    ModelProvider getModelProvider() {
      return modelProvider;
    }
  }
}
