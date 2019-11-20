// Copyright 2019 The Feed Authors.
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

package com.google.android.libraries.feed.common.concurrent.testing;

import com.google.android.libraries.feed.api.internal.common.ThreadUtils;

/** Fake implements of {@link ThreadUtils} that allows callers to control the thread policy. */
public final class FakeThreadUtils extends ThreadUtils {
  private boolean isMainThread = true;
  private boolean enforceThreadChecks = true;

  private FakeThreadUtils(boolean enforceThreadChecks) {
    this.enforceThreadChecks = enforceThreadChecks;
  }

  @Override
  public boolean isMainThread() {
    return isMainThread;
  }

  @Override
  protected void check(boolean condition, String message) {
    if (enforceThreadChecks) {
      super.check(condition, message);
    }
  }

  /**
   * Updates whether the current execution is considered to be on the main thread. Returns the
   * existing thread policy.
   */
  public boolean enforceMainThread(boolean enforceMainThread) {
    boolean result = isMainThread;
    isMainThread = enforceMainThread;
    return result;
  }

  /** Create a {@link FakeThreadUtils} with thread enforcement. */
  public static FakeThreadUtils withThreadChecks() {
    return new FakeThreadUtils(/* enforceThreadChecks= */ true);
  }

  /** Create a {@link FakeThreadUtils} without thread enforcement. */
  public static FakeThreadUtils withoutThreadChecks() {
    return new FakeThreadUtils(/* enforceThreadChecks= */ false);
  }
}
