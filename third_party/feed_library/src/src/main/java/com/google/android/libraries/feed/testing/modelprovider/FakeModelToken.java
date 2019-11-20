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

package com.google.android.libraries.feed.testing.modelprovider;

import com.google.android.libraries.feed.api.internal.modelprovider.ModelToken;
import com.google.android.libraries.feed.api.internal.modelprovider.TokenCompletedObserver;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;
import java.util.HashSet;

/** Fake for {@link ModelToken}. */
public class FakeModelToken implements ModelToken {

  private final StreamToken streamToken;
  private final boolean isSynthetic;
  private final HashSet<TokenCompletedObserver> observers = new HashSet<>();

  private FakeModelToken(StreamToken streamToken, boolean isSynthetic) {
    this.streamToken = streamToken;
    this.isSynthetic = isSynthetic;
  }

  public static Builder newBuilder() {
    return new Builder();
  }

  public HashSet<TokenCompletedObserver> getObservers() {
    return observers;
  }

  @Override
  public StreamToken getStreamToken() {
    return streamToken;
  }

  @Override
  public void registerObserver(TokenCompletedObserver observer) {
    observers.add(observer);
  }

  @Override
  public void unregisterObserver(TokenCompletedObserver observer) {
    observers.remove(observer);
  }

  @Override
  public boolean isSynthetic() {
    return isSynthetic;
  }

  public static class Builder {
    private StreamToken streamToken = StreamToken.getDefaultInstance();
    private boolean isSynthetic;

    private Builder() {}

    public Builder setStreamToken(StreamToken streamToken) {
      this.streamToken = streamToken;
      return this;
    }

    public Builder setIsSynthetic(boolean isSynthetic) {
      this.isSynthetic = isSynthetic;
      return this;
    }

    public FakeModelToken build() {
      return new FakeModelToken(streamToken, isSynthetic);
    }
  }
}
