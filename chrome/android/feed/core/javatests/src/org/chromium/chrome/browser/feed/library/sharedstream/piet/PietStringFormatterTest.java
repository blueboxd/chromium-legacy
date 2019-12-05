// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.piet;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;

import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;

import java.util.concurrent.TimeUnit;

/** Tests the {@link PietStringFormatter} */
public class PietStringFormatterTest {
    @Test
    public void testGetRelativeElapsedString() {
        FakeClock clock = new FakeClock();
        clock.set(TimeUnit.MINUTES.toMillis(1));

        PietStringFormatter pietStringFormatter = new PietStringFormatter(clock);
        assertThat(pietStringFormatter.getRelativeElapsedString(TimeUnit.MINUTES.toMillis(1)))
                .isEqualTo("1 minute ago");
    }
}
