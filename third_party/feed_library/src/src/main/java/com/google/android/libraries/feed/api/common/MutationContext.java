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

package com.google.android.libraries.feed.api.common;

import com.google.search.now.feed.client.StreamDataProto.StreamToken;
import com.google.search.now.feed.client.StreamDataProto.UiContext;

/**
 * This tracks the context of a mutation. When a request is made, this will track the reason for the
 * request and pass this information back with the response.
 */
public final class MutationContext {
  /*@Nullable*/ private final StreamToken continuationToken;
  /*@Nullable*/ private final String requestingSessionId;
  private final boolean userInitiated;

  /** Static used to represent an empty Mutation Context */
  public static final MutationContext EMPTY_CONTEXT =
      new MutationContext(null, null, UiContext.getDefaultInstance(), false);

  private final UiContext uiContext;

  private MutationContext(
      /*@Nullable*/ StreamToken continuationToken,
      /*@Nullable*/ String requestingSessionId,
      UiContext uiContext,
      boolean userInitiated) {
    this.continuationToken = continuationToken;
    this.requestingSessionId = requestingSessionId;
    this.userInitiated = userInitiated;
    this.uiContext = uiContext;
  }

  /** Returns the continuation token used to make the request. */
  /*@Nullable*/
  public StreamToken getContinuationToken() {
    return continuationToken;
  }

  /** Returns the session which made the request. */
  /*@Nullable*/
  public String getRequestingSessionId() {
    return requestingSessionId;
  }

  /**
   * Returns the {@link UiContext} which made the request or {@link UiContext#getDefaultInstance()}
   * if none was present.
   */
  public UiContext getUiContext() {
    return uiContext;
  }

  /** Returns {@code true} if the mutation was user initiated. */
  public boolean isUserInitiated() {
    return userInitiated;
  }

  /** Builder for creating a {@link com.google.android.libraries.feed.api.common.MutationContext */
  public static final class Builder {
    /*@MonotonicNonNull*/ private StreamToken continuationToken;
    /*@MonotonicNonNull*/ private String requestingSessionId;
    private UiContext uiContext = UiContext.getDefaultInstance();
    private boolean userInitiated = false;

    public Builder() {}

    public Builder setContinuationToken(StreamToken continuationToken) {
      this.continuationToken = continuationToken;
      return this;
    }

    public Builder setRequestingSessionId(String requestingSessionId) {
      this.requestingSessionId = requestingSessionId;
      return this;
    }

    public Builder setUserInitiated(boolean userInitiated) {
      this.userInitiated = userInitiated;
      return this;
    }

    public Builder setUiContext(UiContext uiContext) {
      this.uiContext = uiContext;
      return this;
    }

    public MutationContext build() {
      return new MutationContext(continuationToken, requestingSessionId, uiContext, userInitiated);
    }
  }
}
