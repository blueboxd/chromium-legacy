// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import com.google.android.libraries.feed.api.host.config.DebugBehavior;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.functional.Supplier;
import com.google.android.libraries.feed.common.functional.Suppliers;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.common.time.SystemClockImpl;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.common.ui.LayoutUtils;
import com.google.android.libraries.feed.piet.PietStylesHelper.PietStylesHelperFactory;
import com.google.android.libraries.feed.piet.host.ActionHandler;
import com.google.android.libraries.feed.piet.host.AssetProvider;
import com.google.android.libraries.feed.piet.host.CustomElementProvider;
import com.google.android.libraries.feed.piet.host.EventLogger;
import com.google.android.libraries.feed.piet.host.HostBindingProvider;
import com.google.android.libraries.feed.piet.host.ImageLoader;
import com.google.android.libraries.feed.piet.host.LogDataCallback;
import com.google.android.libraries.feed.piet.host.StringFormatter;
import com.google.android.libraries.feed.piet.host.ThrowingCustomElementProvider;
import com.google.android.libraries.feed.piet.host.TypefaceProvider;
import com.google.android.libraries.feed.piet.ui.RoundedCornerMaskCache;
import com.google.search.now.ui.piet.ImagesProto.Image;
import com.google.search.now.ui.piet.LogDataProto.LogData;
import com.google.search.now.ui.piet.PietProto.Frame;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

import java.util.Collections;
import java.util.Locale;

/** Tests of the {@link PietManagerImpl}. */
@RunWith(RobolectricTestRunner.class)
public class PietManagerImplTest {
    @Mock
    private ActionHandler mActionHandler;
    @Mock
    private EventLogger mEventLogger;
    @Mock
    private CustomElementProvider customElementProvider;

    @Mock
    ImageLoader imageLoader;
    @Mock
    StringFormatter stringFormatter;
    @Mock
    TypefaceProvider typefaceProvider;
    @Mock
    private PietStylesHelperFactory stylesHelpers;

    private Context context;
    private ViewGroup viewGroup1;
    private ViewGroup viewGroup2;

    private PietManagerImpl pietManager;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        context = Robolectric.buildActivity(Activity.class).get();
        viewGroup1 = new LinearLayout(context);
        viewGroup2 = new FrameLayout(context);
        pietManager = (PietManagerImpl) PietManager.builder()
                              .setDebugBehavior(DebugBehavior.VERBOSE)
                              .setCustomElementProvider(customElementProvider)
                              .build();
    }

    @Test
    public void testCreatePietFrameAdapter() {
        Supplier<ViewGroup> cardViewSupplier = Suppliers.of(viewGroup1);
        FrameAdapterImpl frameAdapter =
                (FrameAdapterImpl) pietManager.createPietFrameAdapter(cardViewSupplier,
                        mActionHandler, mEventLogger, context, /* logDataCallback= */ null);
        assertThat(frameAdapter.getParameters().parentViewSupplier)
                .isSameInstanceAs(cardViewSupplier);
    }

    @Test
    public void testCreatePietFrameAdapter_loggingCallback() {
        LogDataCallback logDataCallback = new LogDataCallback() {
            @Override
            public void onBind(LogData logData, View view) {}

            @Override
            public void onUnbind(LogData logData, View view) {}
        };
        Supplier<ViewGroup> cardViewSupplier = Suppliers.of(viewGroup1);
        FrameAdapterImpl frameAdapter = (FrameAdapterImpl) pietManager.createPietFrameAdapter(
                cardViewSupplier, mActionHandler, mEventLogger, context, logDataCallback);
        assertThat(frameAdapter.getParameters().hostProviders.getLogDataCallback())
                .isEqualTo(logDataCallback);
    }

    @Test
    public void testGetAdapterParameters() {
        Supplier<ViewGroup> viewGroupProducer1 = Suppliers.of(viewGroup1);
        Supplier<ViewGroup> viewGroupProducer2 = Suppliers.of(viewGroup2);
        Context context1 = Robolectric.buildActivity(Activity.class).get();
        Context context2 = Robolectric.buildActivity(Activity.class).get();

        AdapterParameters returnParams;

        // Get params for a context that does not exist
        returnParams = pietManager.getAdapterParameters(
                context1, viewGroupProducer1, /* logDataCallback= */ null);
        assertThat(returnParams.parentViewSupplier).isEqualTo(viewGroupProducer1);
        assertThat(returnParams.context).isEqualTo(context1);

        // Get params for the same context again (use cached value)
        returnParams = pietManager.getAdapterParameters(
                context1, Suppliers.of(null), /* logDataCallback= */ null);
        assertThat(returnParams.parentViewSupplier).isEqualTo(viewGroupProducer1);

        // Get params for a different context
        returnParams = pietManager.getAdapterParameters(
                context2, viewGroupProducer2, /* logDataCallback= */ null);
        assertThat(returnParams.parentViewSupplier).isEqualTo(viewGroupProducer2);
    }

    @Test
    public void testBuilder_defaults() {
        PietManagerImpl manager = (PietManagerImpl) PietManager.builder().build();
        AdapterParameters parameters = manager.getAdapterParameters(
                context, Suppliers.of(viewGroup1), /* logDataCallback= */ null);
        assertThat(parameters.hostProviders.getCustomElementProvider())
                .isInstanceOf(ThrowingCustomElementProvider.class);
        assertThat(parameters.clock).isInstanceOf(SystemClockImpl.class);
        // There's no good way to test the HostBindingProvider.

        FrameAdapterImpl frameAdapter = (FrameAdapterImpl) manager.createPietFrameAdapter(
                Suppliers.of(viewGroup1), mActionHandler, mEventLogger, context,
                /* logDataCallback= */ null);
        FrameContext frameContext = frameAdapter.createFrameContext(
                Frame.getDefaultInstance(), 0, Collections.emptyList(), viewGroup2);
        assertThat(frameContext.getDebugBehavior()).isEqualTo(DebugBehavior.SILENT);

        AssetProvider assetProvider = manager.assetProvider;

        assertThat(assetProvider.isDarkTheme()).isFalse();
        assertThat(assetProvider.getDefaultCornerRadius()).isEqualTo(0);
        assertThat(assetProvider.getFadeImageThresholdMs()).isEqualTo(Long.MAX_VALUE);
        assertThat(assetProvider.isRtL()).isFalse();
    }

    @Test
    public void testBuilder_rtl() {
        Locale defaultLocale = Locale.getDefault();

        Locale.setDefault(Locale.forLanguageTag("ar"));
        assertThat(LayoutUtils.isDefaultLocaleRtl()).isTrue();

        PietManagerImpl manager = (PietManagerImpl) PietManager.builder().build();

        assertThat(manager.assetProvider.isRtL()).isTrue();

        // Reset the Locale so it doesn't mess up other tests
        // (Removing this causes failures in Kokoro/Bazel testing)
        Locale.setDefault(defaultLocale);
    }

    @Test
    public void testBuilder_setters() {
        boolean isRtL = true;
        boolean isDarkTheme = true;
        HostBindingProvider hostBindingProvider = new HostBindingProvider();
        Clock clock = new FakeClock();
        PietManagerImpl manager = (PietManagerImpl) PietManager.builder()
                                          .setImageLoader(imageLoader)
                                          .setStringFormatter(stringFormatter)
                                          .setDefaultCornerRadius(Suppliers.of(123))
                                          .setIsDarkTheme(Suppliers.of(isDarkTheme))
                                          .setIsRtL(Suppliers.of(isRtL))
                                          .setFadeImageThresholdMs(Suppliers.of(456L))
                                          .setTypefaceProvider(typefaceProvider)
                                          .setDebugBehavior(DebugBehavior.VERBOSE)
                                          .setCustomElementProvider(customElementProvider)
                                          .setHostBindingProvider(hostBindingProvider)
                                          .setClock(clock)
                                          .build();
        AdapterParameters parameters = manager.getAdapterParameters(
                context, Suppliers.of(viewGroup1), /* logDataCallback= */ null);
        assertThat(parameters.hostProviders.getCustomElementProvider())
                .isSameInstanceAs(customElementProvider);
        assertThat(parameters.hostProviders.getHostBindingProvider())
                .isSameInstanceAs(hostBindingProvider);
        assertThat(parameters.clock).isSameInstanceAs(clock);

        FrameAdapterImpl frameAdapter = (FrameAdapterImpl) manager.createPietFrameAdapter(
                Suppliers.of(viewGroup1), mActionHandler, mEventLogger, context,
                /* logDataCallback= */ null);
        FrameContext frameContext = frameAdapter.createFrameContext(
                Frame.getDefaultInstance(), 0, Collections.emptyList(), viewGroup2);
        assertThat(frameContext.getDebugBehavior()).isEqualTo(DebugBehavior.VERBOSE);

        AssetProvider assetProvider = manager.assetProvider;

        Consumer<Drawable> drawableConsumer = drawable -> {};
        assetProvider.getImage(Image.getDefaultInstance(), 12, 34, drawableConsumer);
        verify(imageLoader).getImage(Image.getDefaultInstance(), 12, 34, drawableConsumer);

        assetProvider.getRelativeElapsedString(789);
        verify(stringFormatter).getRelativeElapsedString(789);

        Consumer<Typeface> typefaceConsumer = typeface -> {};
        assetProvider.getTypeface("blah", false, typefaceConsumer);
        verify(typefaceProvider).getTypeface("blah", false, typefaceConsumer);

        assertThat(assetProvider.isDarkTheme()).isEqualTo(isDarkTheme);
        assertThat(assetProvider.isRtL()).isEqualTo(isRtL);

        assertThat(assetProvider.getDefaultCornerRadius()).isEqualTo(123);
        assertThat(assetProvider.getFadeImageThresholdMs()).isEqualTo(456);
    }

    @Test
    public void testPurgeRecyclerPools() {
        // Test with null adapterParameters
        pietManager.purgeRecyclerPools();

        ElementAdapterFactory mockFactory = mock(ElementAdapterFactory.class);
        TemplateBinder mockTemplateBinder = mock(TemplateBinder.class);
        HostProviders hostProviders = mock(HostProviders.class);
        RoundedCornerMaskCache maskCache = mock(RoundedCornerMaskCache.class);
        pietManager.adapterParameters = new AdapterParameters(context, Suppliers.of(viewGroup1),
                hostProviders, null, mockFactory, mockTemplateBinder, new FakeClock(),
                stylesHelpers, maskCache, false, false);
        pietManager.purgeRecyclerPools();
        verify(mockFactory).purgeRecyclerPools();
        verify(stylesHelpers).purge();
        verify(maskCache).purge();
    }
}
