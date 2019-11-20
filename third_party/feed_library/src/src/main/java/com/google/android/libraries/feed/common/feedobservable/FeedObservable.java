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

package com.google.android.libraries.feed.common.feedobservable;

import com.google.android.libraries.feed.common.logging.Logger;
import java.util.HashSet;
import java.util.Set;

/** Provides methods for registering or unregistering arbitrary observers. */
public abstract class FeedObservable<ObserverT> implements Observable<ObserverT> {
  private static final String TAG = "FeedObservable";

  protected final Set<ObserverT> observers = new HashSet<>();

  /** Adds given {@code observer}. No-op if the observer has already been added. */
  @Override
  public void registerObserver(ObserverT observer) {
    synchronized (observers) {
      if (!observers.add(observer)) {
        Logger.w(TAG, "Registering observer: %s multiple times.", observer);
      }
    }
  }

  /** Removes given {@code observer}. No-op if the observer is not currently added. */
  @Override
  public void unregisterObserver(ObserverT observer) {
    synchronized (observers) {
      if (!observers.remove(observer)) {
        Logger.w(TAG, "Unregistering observer: %s that isn't registered.", observer);
      }
    }
  }
}
