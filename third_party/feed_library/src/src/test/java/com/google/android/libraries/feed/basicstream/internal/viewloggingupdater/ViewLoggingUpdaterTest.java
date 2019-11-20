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

package com.google.android.libraries.feed.basicstream.internal.viewloggingupdater;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link ViewLoggingUpdater}. */
@RunWith(RobolectricTestRunner.class)
public class ViewLoggingUpdaterTest {

  @Test
  public void testResetViewTracking_resetsListeners() {
    ResettableOneShotVisibilityLoggingListener listerner1 =
        mock(ResettableOneShotVisibilityLoggingListener.class);
    ResettableOneShotVisibilityLoggingListener listerner2 =
        mock(ResettableOneShotVisibilityLoggingListener.class);

    ViewLoggingUpdater viewLoggingUpdater = new ViewLoggingUpdater();

    viewLoggingUpdater.registerObserver(listerner1);
    viewLoggingUpdater.registerObserver(listerner2);

    viewLoggingUpdater.resetViewTracking();

    verify(listerner1).reset();
    verify(listerner2).reset();
  }
}
