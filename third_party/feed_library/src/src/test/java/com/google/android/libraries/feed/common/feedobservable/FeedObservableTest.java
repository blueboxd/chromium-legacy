// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.feedobservable;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link com.google.android.libraries.feed.common.feedobservable.FeedObservable}. */
@RunWith(RobolectricTestRunner.class)
public class FeedObservableTest {
    private FeedObservableForTest observable;

    @Before
    public void setup() {
        observable = new FeedObservableForTest();
    }

    @Test
    public void updateObservers_updatesRegisteredObservers() {
        Observer observer = new Observer();
        observable.registerObserver(observer);

        observable.updateObservers();

        assertThat(observer.updateCount).isEqualTo(1);
    }

    @Test
    public void updateObservers_doesntUpdateUnregisteredObservers() {
        Observer observer = new Observer();
        observable.registerObserver(observer);
        observable.unregisterObserver(observer);

        observable.updateObservers();

        assertThat(observer.updateCount).isEqualTo(0);
    }

    @Test
    public void updateObservers_multipleObservers_updatesAll() {
        Observer observer1 = new Observer();
        Observer observer2 = new Observer();
        observable.registerObserver(observer1);
        observable.registerObserver(observer2);

        observable.updateObservers();

        assertThat(observer1.updateCount).isEqualTo(1);
        assertThat(observer2.updateCount).isEqualTo(1);
    }

    @Test
    public void updateObservers_doubleRegister_updatesOnce() {
        Observer observer = new Observer();
        observable.registerObserver(observer);
        observable.registerObserver(observer);

        observable.updateObservers();

        assertThat(observer.updateCount).isEqualTo(1);
    }

    @Test
    public void unRegisterObserver_nonRegisteredObserver_doesntCrash() {
        // Should not crash.
        observable.unregisterObserver(new Observer());
    }

    private static class FeedObservableForTest extends FeedObservable<Observer> {
        void updateObservers() {
            for (Observer observer : observers) {
                observer.update();
            }
        }
    }

    private static class Observer {
        private int updateCount;

        void update() {
            updateCount++;
        }
    }
}
