// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

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

import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Consumer;
import org.chromium.base.Supplier;
import org.chromium.chrome.browser.feed.library.api.host.config.DebugBehavior;
import org.chromium.chrome.browser.feed.library.common.functional.Suppliers;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.common.time.SystemClockImpl;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.common.ui.LayoutUtils;
import org.chromium.chrome.browser.feed.library.piet.PietStylesHelper.PietStylesHelperFactory;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler;
import org.chromium.chrome.browser.feed.library.piet.host.AssetProvider;
import org.chromium.chrome.browser.feed.library.piet.host.CustomElementProvider;
import org.chromium.chrome.browser.feed.library.piet.host.EventLogger;
import org.chromium.chrome.browser.feed.library.piet.host.HostBindingProvider;
import org.chromium.chrome.browser.feed.library.piet.host.ImageLoader;
import org.chromium.chrome.browser.feed.library.piet.host.LogDataCallback;
import org.chromium.chrome.browser.feed.library.piet.host.StringFormatter;
import org.chromium.chrome.browser.feed.library.piet.host.ThrowingCustomElementProvider;
import org.chromium.chrome.browser.feed.library.piet.host.TypefaceProvider;
import org.chromium.chrome.browser.feed.library.piet.ui.RoundedCornerMaskCache;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.Image;
import org.chromium.components.feed.core.proto.ui.piet.LogDataProto.LogData;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Frame;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Collections;
import java.util.Locale;

/** Tests of the {@link PietManagerImpl}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Ignore("crbug.com/1024945 Test does not work")
public class PietManagerImplTest {
    @Mock
    private ActionHandler mActionHandler;
    @Mock
    private EventLogger mEventLogger;
    @Mock
    private CustomElementProvider mCustomElementProvider;

    @Mock
    ImageLoader mImageLoader;
    @Mock
    StringFormatter mStringFormatter;
    @Mock
    TypefaceProvider mTypefaceProvider;
    @Mock
    private PietStylesHelperFactory mStylesHelpers;

    private Context mContext;
    private ViewGroup mViewGroup1;
    private ViewGroup mViewGroup2;

    private PietManagerImpl mPietManager;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        mViewGroup1 = new LinearLayout(mContext);
        mViewGroup2 = new FrameLayout(mContext);
        mPietManager = (PietManagerImpl) new PietManager.Builder()
                               .setDebugBehavior(DebugBehavior.VERBOSE)
                               .setCustomElementProvider(mCustomElementProvider)
                               .build();
    }

    @Test
    public void testCreatePietFrameAdapter() {
        Supplier<ViewGroup> cardViewSupplier = Suppliers.of(mViewGroup1);
        FrameAdapterImpl frameAdapter =
                (FrameAdapterImpl) mPietManager.createPietFrameAdapter(cardViewSupplier,
                        mActionHandler, mEventLogger, mContext, /* logDataCallback= */ null);
        assertThat(frameAdapter.getParameters().mParentViewSupplier)
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
        Supplier<ViewGroup> cardViewSupplier = Suppliers.of(mViewGroup1);
        FrameAdapterImpl frameAdapter = (FrameAdapterImpl) mPietManager.createPietFrameAdapter(
                cardViewSupplier, mActionHandler, mEventLogger, mContext, logDataCallback);
        assertThat(frameAdapter.getParameters().mHostProviders.getLogDataCallback())
                .isEqualTo(logDataCallback);
    }

    @Test
    public void testGetAdapterParameters() {
        Supplier<ViewGroup> viewGroupProducer1 = Suppliers.of(mViewGroup1);
        Supplier<ViewGroup> viewGroupProducer2 = Suppliers.of(mViewGroup2);
        Context context1 = Robolectric.buildActivity(Activity.class).get();
        Context context2 = Robolectric.buildActivity(Activity.class).get();

        AdapterParameters returnParams;

        // Get params for a context that does not exist
        returnParams = mPietManager.getAdapterParameters(
                context1, viewGroupProducer1, /* logDataCallback= */ null);
        assertThat(returnParams.mParentViewSupplier).isEqualTo(viewGroupProducer1);
        assertThat(returnParams.mContext).isEqualTo(context1);

        // Get params for the same context again (use cached value)
        returnParams = mPietManager.getAdapterParameters(
                context1, Suppliers.of(null), /* logDataCallback= */ null);
        assertThat(returnParams.mParentViewSupplier).isEqualTo(viewGroupProducer1);

        // Get params for a different context
        returnParams = mPietManager.getAdapterParameters(
                context2, viewGroupProducer2, /* logDataCallback= */ null);
        assertThat(returnParams.mParentViewSupplier).isEqualTo(viewGroupProducer2);
    }

    @Test
    public void testBuilder_defaults() {
        PietManagerImpl manager = (PietManagerImpl) new PietManager.Builder().build();
        AdapterParameters parameters = manager.getAdapterParameters(
                mContext, Suppliers.of(mViewGroup1), /* logDataCallback= */ null);
        assertThat(parameters.mHostProviders.getCustomElementProvider())
                .isInstanceOf(ThrowingCustomElementProvider.class);
        assertThat(parameters.mClock).isInstanceOf(SystemClockImpl.class);
        // There's no good way to test the HostBindingProvider.

        FrameAdapterImpl frameAdapter = (FrameAdapterImpl) manager.createPietFrameAdapter(
                Suppliers.of(mViewGroup1), mActionHandler, mEventLogger, mContext,
                /* logDataCallback= */ null);
        FrameContext frameContext = frameAdapter.createFrameContext(
                Frame.getDefaultInstance(), 0, Collections.emptyList(), mViewGroup2);
        assertThat(frameContext.getDebugBehavior()).isEqualTo(DebugBehavior.SILENT);

        AssetProvider assetProvider = manager.mAssetProvider;

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

        PietManagerImpl manager = (PietManagerImpl) new PietManager.Builder().build();

        assertThat(manager.mAssetProvider.isRtL()).isTrue();

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
        PietManagerImpl manager = (PietManagerImpl) new PietManager.Builder()
                                          .setImageLoader(mImageLoader)
                                          .setStringFormatter(mStringFormatter)
                                          .setDefaultCornerRadius(Suppliers.of(123))
                                          .setIsDarkTheme(Suppliers.of(isDarkTheme))
                                          .setIsRtL(Suppliers.of(isRtL))
                                          .setFadeImageThresholdMs(Suppliers.of(456L))
                                          .setTypefaceProvider(mTypefaceProvider)
                                          .setDebugBehavior(DebugBehavior.VERBOSE)
                                          .setCustomElementProvider(mCustomElementProvider)
                                          .setHostBindingProvider(hostBindingProvider)
                                          .setClock(clock)
                                          .build();
        AdapterParameters parameters = manager.getAdapterParameters(
                mContext, Suppliers.of(mViewGroup1), /* logDataCallback= */ null);
        assertThat(parameters.mHostProviders.getCustomElementProvider())
                .isSameInstanceAs(mCustomElementProvider);
        assertThat(parameters.mHostProviders.getHostBindingProvider())
                .isSameInstanceAs(hostBindingProvider);
        assertThat(parameters.mClock).isSameInstanceAs(clock);

        FrameAdapterImpl frameAdapter = (FrameAdapterImpl) manager.createPietFrameAdapter(
                Suppliers.of(mViewGroup1), mActionHandler, mEventLogger, mContext,
                /* logDataCallback= */ null);
        FrameContext frameContext = frameAdapter.createFrameContext(
                Frame.getDefaultInstance(), 0, Collections.emptyList(), mViewGroup2);
        assertThat(frameContext.getDebugBehavior()).isEqualTo(DebugBehavior.VERBOSE);

        AssetProvider assetProvider = manager.mAssetProvider;

        Consumer<Drawable> drawableConsumer = drawable -> {};
        assetProvider.getImage(Image.getDefaultInstance(), 12, 34, drawableConsumer);
        verify(mImageLoader).getImage(Image.getDefaultInstance(), 12, 34, drawableConsumer);

        assetProvider.getRelativeElapsedString(789);
        verify(mStringFormatter).getRelativeElapsedString(789);

        Consumer<Typeface> typefaceConsumer = typeface -> {};
        assetProvider.getTypeface("blah", false, typefaceConsumer);
        verify(mTypefaceProvider).getTypeface("blah", false, typefaceConsumer);

        assertThat(assetProvider.isDarkTheme()).isEqualTo(isDarkTheme);
        assertThat(assetProvider.isRtL()).isEqualTo(isRtL);

        assertThat(assetProvider.getDefaultCornerRadius()).isEqualTo(123);
        assertThat(assetProvider.getFadeImageThresholdMs()).isEqualTo(456);
    }

    @Test
    public void testPurgeRecyclerPools() {
        // Test with null adapterParameters
        mPietManager.purgeRecyclerPools();

        ElementAdapterFactory mockFactory = mock(ElementAdapterFactory.class);
        TemplateBinder mockTemplateBinder = mock(TemplateBinder.class);
        HostProviders hostProviders = mock(HostProviders.class);
        RoundedCornerMaskCache maskCache = mock(RoundedCornerMaskCache.class);
        mPietManager.mAdapterParameters = new AdapterParameters(mContext, Suppliers.of(mViewGroup1),
                hostProviders, null, mockFactory, mockTemplateBinder, new FakeClock(),
                mStylesHelpers, maskCache, false, false);
        mPietManager.purgeRecyclerPools();
        verify(mockFactory).purgeRecyclerPools();
        verify(mStylesHelpers).purge();
        verify(maskCache).purge();
    }
}
