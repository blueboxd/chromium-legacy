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

package com.google.android.libraries.feed.api.internal.modelprovider;

import android.support.annotation.IntDef;
import com.google.protobuf.ByteString;

/**
 * Class which contains information needed by the {@link
 * com.google.android.libraries.feed.api.client.stream.Stream} about errors which may occur in the
 * infrastructure.
 */
public final class ModelError {

  /** Defines errors are exposed through the ModelProvider to the Stream. */
  @IntDef({ErrorType.UNKNOWN, ErrorType.NO_CARDS_ERROR, ErrorType.PAGINATION_ERROR})
  public @interface ErrorType {
    // An unknown error, this is not expected to ever be used.
    int UNKNOWN = 0;
    // No cards are available due to an error such as, no network available or a request failed,
    // etc.
    int NO_CARDS_ERROR = 1;
    // Pagination failed due to some type of error such as no network available or a request failed,
    // etc.
    int PAGINATION_ERROR = 2;
    // Pagination failed due to a synthetic token error.
    int SYNTHETIC_TOKEN_ERROR = 3;
  }

  private final @ErrorType int errorType;
  /*@Nullable*/ private final ByteString continuationToken;

  public ModelError(@ErrorType int errorType, /*@Nullable*/ ByteString continuationToken) {
    this.errorType = errorType;
    this.continuationToken = continuationToken;
  }

  /** Returns the ErrorType assocated with the error. */
  public @ErrorType int getErrorType() {
    return errorType;
  }

  /** This should be non-null if the ErrorType is PAGINATION_ERROR. */
  /*@Nullable*/
  public ByteString getContinuationToken() {
    return continuationToken;
  }
}
