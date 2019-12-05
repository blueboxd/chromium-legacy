// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.feedobservable;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Tests for {@link org.chromium.chrome.browser.feed.library.common.feedobservable.FeedObservable}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedObservableTest {
    private FeedObservableForTest mObservable;

    @Before
    public void setup() {
        mObservable = new FeedObservableForTest();
    }

    @Test
    public void updateObservers_updatesRegisteredObservers() {
        Observer observer = new Observer();
        mObservable.registerObserver(observer);

        mObservable.updateObservers();

        assertThat(observer.mUpdateCount).isEqualTo(1);
    }

    @Test
    public void updateObservers_doesntUpdateUnregisteredObservers() {
        Observer observer = new Observer();
        mObservable.registerObserver(observer);
        mObservable.unregisterObserver(observer);

        mObservable.updateObservers();

        assertThat(observer.mUpdateCount).isEqualTo(0);
    }

    @Test
    public void updateObservers_multipleObservers_updatesAll() {
        Observer observer1 = new Observer();
        Observer observer2 = new Observer();
        mObservable.registerObserver(observer1);
        mObservable.registerObserver(observer2);

        mObservable.updateObservers();

        assertThat(observer1.mUpdateCount).isEqualTo(1);
        assertThat(observer2.mUpdateCount).isEqualTo(1);
    }

    @Test
    public void updateObservers_doubleRegister_updatesOnce() {
        Observer observer = new Observer();
        mObservable.registerObserver(observer);
        mObservable.registerObserver(observer);

        mObservable.updateObservers();

        assertThat(observer.mUpdateCount).isEqualTo(1);
    }

    @Test
    public void unRegisterObserver_nonRegisteredObserver_doesntCrash() {
        // Should not crash.
        mObservable.unregisterObserver(new Observer());
    }

    private static class FeedObservableForTest extends FeedObservable<Observer> {
        void updateObservers() {
            for (Observer observer : mObservers) {
                observer.update();
            }
        }
    }

    private static class Observer {
        private int mUpdateCount;

        void update() {
            mUpdateCount++;
        }
    }
}
