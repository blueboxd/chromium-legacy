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

import com.google.android.libraries.feed.common.functional.Consumer;

/**
 * A FeedObservable implementation that allows calling a notify method on all observers, and
 * immediately triggers observers when they are registered.
 */
public final class ImmediatelyNotifyingObservableNotifier<ObserverT>
    extends FeedObservable<ObserverT> {

  private Consumer<ObserverT> currentConsumer;

  public ImmediatelyNotifyingObservableNotifier(Consumer<ObserverT> listenerMethod) {
    currentConsumer = listenerMethod;
  }

  @Override
  public void registerObserver(ObserverT observerT) {
    super.registerObserver(observerT);
    currentConsumer.accept(observerT);
  }

  /** Calls all the registered listeners using the given listener method. */
  public void notifyListeners(Consumer<ObserverT> listenerMethod) {
    currentConsumer = listenerMethod;
    synchronized (observers) {
      for (ObserverT listener : observers) {
        listenerMethod.accept(listener);
      }
    }
  }
}
