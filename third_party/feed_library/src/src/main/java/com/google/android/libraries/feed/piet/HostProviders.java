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

package com.google.android.libraries.feed.piet;

import com.google.android.libraries.feed.piet.host.AssetProvider;
import com.google.android.libraries.feed.piet.host.CustomElementProvider;
import com.google.android.libraries.feed.piet.host.HostBindingProvider;
import com.google.android.libraries.feed.piet.host.LogDataCallback;

/** Wrapper class to hold all host-related objects. */
public class HostProviders {
  private final AssetProvider assetProvider;
  private final CustomElementProvider customElementProvider;
  private final HostBindingProvider hostBindingProvider;
  /*@Nullable*/ private final LogDataCallback logDataCallback;

  public HostProviders(
      AssetProvider assetProvider,
      CustomElementProvider customElementProvider,
      HostBindingProvider hostBindingProvider,
      /*@Nullable*/ LogDataCallback logDataCallback) {
    this.assetProvider = assetProvider;
    this.customElementProvider = customElementProvider;
    this.hostBindingProvider = hostBindingProvider;
    this.logDataCallback = logDataCallback;
  }

  public AssetProvider getAssetProvider() {
    return assetProvider;
  }

  public CustomElementProvider getCustomElementProvider() {
    return customElementProvider;
  }

  public HostBindingProvider getHostBindingProvider() {
    return hostBindingProvider;
  }

  /*@Nullable*/
  public LogDataCallback getLogDataCallback() {
    return logDataCallback;
  }
}
