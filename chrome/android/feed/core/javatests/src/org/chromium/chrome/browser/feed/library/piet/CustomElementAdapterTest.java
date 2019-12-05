// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;

import android.app.Activity;
import android.content.Context;
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.common.functional.Suppliers;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.piet.CustomElementAdapter.KeySupplier;
import org.chromium.chrome.browser.feed.library.piet.host.AssetProvider;
import org.chromium.chrome.browser.feed.library.piet.host.CustomElementProvider;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.CustomBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingValue;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.CustomElement;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.CustomElementData;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.TextElement;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.StyleIdsStack;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link CustomElementAdapter}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CustomElementAdapterTest {
    private static final CustomElementData DUMMY_DATA = CustomElementData.getDefaultInstance();
    private static final Element DEFAULT_MODEL =
            asElement(CustomElement.newBuilder().setCustomElementData(DUMMY_DATA).build());

    @Mock
    private FrameContext mFrameContext;
    @Mock
    private CustomElementProvider mCustomElementProvider;
    @Mock
    private HostProviders mHostProviders;
    @Mock
    private AssetProvider mAssetProvider;

    private Context mContext;
    private View mCustomView;
    private CustomElementAdapter mAdapter;
    private AdapterParameters mAdapterParameters;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        mCustomView = new View(mContext);

        when(mHostProviders.getCustomElementProvider()).thenReturn(mCustomElementProvider);
        when(mHostProviders.getAssetProvider()).thenReturn(mAssetProvider);
        when(mAssetProvider.isRtL()).thenReturn(false);
        when(mCustomElementProvider.createCustomElement(DUMMY_DATA)).thenReturn(mCustomView);
        when(mFrameContext.reportMessage(anyInt(), any(), anyString()))
                .thenAnswer(invocation -> invocation.getArguments()[2]);

        mAdapterParameters = new AdapterParameters(
                mContext, Suppliers.of(null), mHostProviders, new FakeClock(), false, false);

        when(mFrameContext.makeStyleFor(any(StyleIdsStack.class)))
                .thenReturn(mAdapterParameters.mDefaultStyleProvider);

        mAdapter = new KeySupplier().getAdapter(mContext, mAdapterParameters);
    }

    @Test
    public void testCreate() {
        assertThat(mAdapter).isNotNull();
    }

    @Test
    public void testCreateAdapter_initializes() {
        mAdapter.createAdapter(DEFAULT_MODEL, mFrameContext);

        assertThat(mAdapter.getView()).isNotNull();
        assertThat(mAdapter.getKey()).isNotNull();
    }

    @Test
    public void testCreateAdapter_ignoresSubsequentCalls() {
        mAdapter.createAdapter(DEFAULT_MODEL, mFrameContext);
        View adapterView = mAdapter.getView();
        RecyclerKey adapterKey = mAdapter.getKey();

        mAdapter.createAdapter(DEFAULT_MODEL, mFrameContext);

        assertThat(mAdapter.getView()).isSameInstanceAs(adapterView);
        assertThat(mAdapter.getKey()).isSameInstanceAs(adapterKey);
    }

    @Test
    public void testBindModel_data() {
        mAdapter.createAdapter(DEFAULT_MODEL, mFrameContext);
        mAdapter.bindModel(DEFAULT_MODEL, mFrameContext);

        assertThat(mAdapter.getBaseView().getChildAt(0)).isEqualTo(mCustomView);
    }

    @Test
    public void testBindModel_binding() {
        CustomBindingRef bindingRef = CustomBindingRef.newBuilder().setBindingId("CUSTOM!").build();
        when(mFrameContext.getCustomElementBindingValue(bindingRef))
                .thenReturn(BindingValue.newBuilder().setCustomElementData(DUMMY_DATA).build());

        Element model = asElement(CustomElement.newBuilder().setCustomBinding(bindingRef).build());

        mAdapter.createAdapter(model, mFrameContext);
        mAdapter.bindModel(model, mFrameContext);

        assertThat(mAdapter.getBaseView().getChildAt(0)).isEqualTo(mCustomView);
    }

    @Test
    public void testBindModel_noContent() {
        mAdapter.createAdapter(DEFAULT_MODEL, mFrameContext);
        mAdapter.bindModel(asElement(CustomElement.getDefaultInstance()), mFrameContext);

        verifyZeroInteractions(mCustomElementProvider);
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
    }

    @Test
    public void testBindModel_optionalAbsent() {
        CustomBindingRef bindingRef =
                CustomBindingRef.newBuilder().setBindingId("CUSTOM!").setIsOptional(true).build();
        when(mFrameContext.getCustomElementBindingValue(bindingRef))
                .thenReturn(BindingValue.getDefaultInstance());
        Element model = asElement(CustomElement.newBuilder().setCustomBinding(bindingRef).build());
        mAdapter.createAdapter(model, mFrameContext);

        // This should not fail.
        mAdapter.bindModel(model, mFrameContext);
        assertThat(mAdapter.getView().getVisibility()).isEqualTo(View.GONE);
    }

    @Test
    public void testBindModel_noContentInBinding() {
        CustomBindingRef bindingRef = CustomBindingRef.newBuilder().setBindingId("CUSTOM").build();
        when(mFrameContext.getCustomElementBindingValue(bindingRef))
                .thenReturn(BindingValue.newBuilder().setBindingId("CUSTOM").build());
        Element model = asElement(CustomElement.newBuilder().setCustomBinding(bindingRef).build());
        mAdapter.createAdapter(model, mFrameContext);

        assertThatRunnable(() -> mAdapter.bindModel(model, mFrameContext))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Custom element binding CUSTOM had no content");
    }

    @Test
    public void testUnbindModel() {
        mAdapter.createAdapter(DEFAULT_MODEL, mFrameContext);
        mAdapter.bindModel(DEFAULT_MODEL, mFrameContext);

        mAdapter.unbindModel();

        verify(mCustomElementProvider).releaseCustomView(mCustomView, DUMMY_DATA);
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
    }

    @Test
    public void testUnbindModel_noChildren() {
        mAdapter.createAdapter(DEFAULT_MODEL, mFrameContext);

        mAdapter.unbindModel();

        verifyZeroInteractions(mCustomElementProvider);
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
    }

    @Test
    public void testUnbindModel_multipleChildren() {
        View customView2 = new View(mContext);
        when(mCustomElementProvider.createCustomElement(DUMMY_DATA))
                .thenReturn(mCustomView)
                .thenReturn(customView2);

        mAdapter.createAdapter(DEFAULT_MODEL, mFrameContext);
        mAdapter.bindModel(DEFAULT_MODEL, mFrameContext);
        mAdapter.bindModel(DEFAULT_MODEL, mFrameContext);

        mAdapter.unbindModel();

        verify(mCustomElementProvider).releaseCustomView(mCustomView, DUMMY_DATA);
        verify(mCustomElementProvider).releaseCustomView(customView2, DUMMY_DATA);
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
    }

    @Test
    public void testGetModelFromElement() {
        CustomElement model = CustomElement.newBuilder().build();

        Element elementWithModel = Element.newBuilder().setCustomElement(model).build();
        assertThat(mAdapter.getModelFromElement(elementWithModel)).isSameInstanceAs(model);

        Element elementWithWrongModel =
                Element.newBuilder().setTextElement(TextElement.getDefaultInstance()).build();
        assertThatRunnable(() -> mAdapter.getModelFromElement(elementWithWrongModel))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Missing CustomElement");

        Element emptyElement = Element.getDefaultInstance();
        assertThatRunnable(() -> mAdapter.getModelFromElement(emptyElement))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Missing CustomElement");
    }

    private static Element asElement(CustomElement customElement) {
        return Element.newBuilder().setCustomElement(customElement).build();
    }
}
