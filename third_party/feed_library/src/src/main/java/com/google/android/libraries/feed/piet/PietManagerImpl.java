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

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import android.view.ViewGroup;
import com.google.android.libraries.feed.api.host.config.DebugBehavior;
import com.google.android.libraries.feed.common.functional.Supplier;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.piet.host.ActionHandler;
import com.google.android.libraries.feed.piet.host.AssetProvider;
import com.google.android.libraries.feed.piet.host.CustomElementProvider;
import com.google.android.libraries.feed.piet.host.EventLogger;
import com.google.android.libraries.feed.piet.host.HostBindingProvider;
import com.google.android.libraries.feed.piet.host.LogDataCallback;

/** Implementation of {@link PietManager}. */
class PietManagerImpl implements PietManager {

  @VisibleForTesting final AssetProvider assetProvider;
  private final DebugBehavior debugBehavior;
  private final CustomElementProvider customElementProvider;
  private final HostBindingProvider hostBindingProvider;
  private final Clock clock;
  private final boolean allowLegacyRoundedCornerImpl;
  private final boolean allowOutlineRoundedCornerImpl;
  @VisibleForTesting /*@Nullable*/ AdapterParameters adapterParameters = null;

  PietManagerImpl(
      DebugBehavior debugBehavior,
      AssetProvider assetProvider,
      CustomElementProvider customElementProvider,
      HostBindingProvider hostBindingProvider,
      Clock clock,
      boolean allowLegacyRoundedCornerImpl,
      boolean allowOutlineRoundedCornerImpl) {
    this.debugBehavior = debugBehavior;
    this.assetProvider = assetProvider;
    this.customElementProvider = customElementProvider;
    this.hostBindingProvider = hostBindingProvider;
    this.clock = clock;
    this.allowLegacyRoundedCornerImpl = allowLegacyRoundedCornerImpl;
    this.allowOutlineRoundedCornerImpl = allowOutlineRoundedCornerImpl;
  }

  @Override
  public FrameAdapter createPietFrameAdapter(
      Supplier</*@Nullable*/ ViewGroup> cardViewProducer,
      ActionHandler actionHandler,
      EventLogger eventLogger,
      Context context) {
    return createPietFrameAdapter(cardViewProducer, actionHandler, eventLogger, context, null);
  }

  @Override
  public FrameAdapter createPietFrameAdapter(
      Supplier</*@Nullable*/ ViewGroup> cardViewProducer,
      ActionHandler actionHandler,
      EventLogger eventLogger,
      Context context,
      /*@Nullable*/ LogDataCallback logDataCallback) {
    AdapterParameters parameters = getAdapterParameters(context, cardViewProducer, logDataCallback);

    return new FrameAdapterImpl(context, parameters, actionHandler, eventLogger, debugBehavior);
  }

  /**
   * Return the {@link AdapterParameters}. If one doesn't exist this will create a new instance. The
   * {@code AdapterParameters} is scoped to the {@code Context}.
   */
  @VisibleForTesting
  AdapterParameters getAdapterParameters(
      Context context,
      Supplier</*@Nullable*/ ViewGroup> cardViewProducer,
      /*@Nullable*/ LogDataCallback logDataCallback) {
    if (adapterParameters == null || adapterParameters.context != context) {
      adapterParameters =
          new AdapterParameters(
              context,
              cardViewProducer,
              new HostProviders(
                  assetProvider, customElementProvider, hostBindingProvider, logDataCallback),
              clock,
              allowLegacyRoundedCornerImpl,
              allowOutlineRoundedCornerImpl);
    }
    return adapterParameters;
  }

  @Override
  public void purgeRecyclerPools() {
    if (adapterParameters != null) {
      AdapterParameters adapterParametersNonNull = adapterParameters;
      adapterParametersNonNull.elementAdapterFactory.purgeRecyclerPools();
      adapterParametersNonNull.pietStylesHelperFactory.purge();
      adapterParametersNonNull.roundedCornerMaskCache.purge();
    }
  }
}
