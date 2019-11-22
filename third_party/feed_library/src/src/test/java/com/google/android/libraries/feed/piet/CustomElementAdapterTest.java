// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.view.View;

import com.google.android.libraries.feed.common.functional.Suppliers;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.piet.CustomElementAdapter.KeySupplier;
import com.google.android.libraries.feed.piet.host.AssetProvider;
import com.google.android.libraries.feed.piet.host.CustomElementProvider;
import com.google.search.now.ui.piet.BindingRefsProto.CustomBindingRef;
import com.google.search.now.ui.piet.ElementsProto.BindingValue;
import com.google.search.now.ui.piet.ElementsProto.CustomElement;
import com.google.search.now.ui.piet.ElementsProto.CustomElementData;
import com.google.search.now.ui.piet.ElementsProto.Element;
import com.google.search.now.ui.piet.ElementsProto.TextElement;
import com.google.search.now.ui.piet.StylesProto.StyleIdsStack;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests of the {@link CustomElementAdapter}. */
@RunWith(RobolectricTestRunner.class)
public class CustomElementAdapterTest {
    private static final CustomElementData DUMMY_DATA = CustomElementData.getDefaultInstance();
    private static final Element DEFAULT_MODEL =
            asElement(CustomElement.newBuilder().setCustomElementData(DUMMY_DATA).build());

    @Mock
    private FrameContext frameContext;
    @Mock
    private CustomElementProvider customElementProvider;
    @Mock
    private HostProviders hostProviders;
    @Mock
    private AssetProvider assetProvider;

    private Context context;
    private View customView;
    private CustomElementAdapter adapter;
    private AdapterParameters adapterParameters;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        context = Robolectric.buildActivity(Activity.class).get();
        customView = new View(context);

        when(hostProviders.getCustomElementProvider()).thenReturn(customElementProvider);
        when(hostProviders.getAssetProvider()).thenReturn(assetProvider);
        when(assetProvider.isRtL()).thenReturn(false);
        when(customElementProvider.createCustomElement(DUMMY_DATA)).thenReturn(customView);
        when(frameContext.reportMessage(anyInt(), any(), anyString()))
                .thenAnswer(invocation -> invocation.getArguments()[2]);

        adapterParameters = new AdapterParameters(
                context, Suppliers.of(null), hostProviders, new FakeClock(), false, false);

        when(frameContext.makeStyleFor(any(StyleIdsStack.class)))
                .thenReturn(adapterParameters.defaultStyleProvider);

        adapter = new KeySupplier().getAdapter(context, adapterParameters);
    }

    @Test
    public void testCreate() {
        assertThat(adapter).isNotNull();
    }

    @Test
    public void testCreateAdapter_initializes() {
        adapter.createAdapter(DEFAULT_MODEL, frameContext);

        assertThat(adapter.getView()).isNotNull();
        assertThat(adapter.getKey()).isNotNull();
    }

    @Test
    public void testCreateAdapter_ignoresSubsequentCalls() {
        adapter.createAdapter(DEFAULT_MODEL, frameContext);
        View adapterView = adapter.getView();
        RecyclerKey adapterKey = adapter.getKey();

        adapter.createAdapter(DEFAULT_MODEL, frameContext);

        assertThat(adapter.getView()).isSameInstanceAs(adapterView);
        assertThat(adapter.getKey()).isSameInstanceAs(adapterKey);
    }

    @Test
    public void testBindModel_data() {
        adapter.createAdapter(DEFAULT_MODEL, frameContext);
        adapter.bindModel(DEFAULT_MODEL, frameContext);

        assertThat(adapter.getBaseView().getChildAt(0)).isEqualTo(customView);
    }

    @Test
    public void testBindModel_binding() {
        CustomBindingRef bindingRef = CustomBindingRef.newBuilder().setBindingId("CUSTOM!").build();
        when(frameContext.getCustomElementBindingValue(bindingRef))
                .thenReturn(BindingValue.newBuilder().setCustomElementData(DUMMY_DATA).build());

        Element model = asElement(CustomElement.newBuilder().setCustomBinding(bindingRef).build());

        adapter.createAdapter(model, frameContext);
        adapter.bindModel(model, frameContext);

        assertThat(adapter.getBaseView().getChildAt(0)).isEqualTo(customView);
    }

    @Test
    public void testBindModel_noContent() {
        adapter.createAdapter(DEFAULT_MODEL, frameContext);
        adapter.bindModel(asElement(CustomElement.getDefaultInstance()), frameContext);

        verifyZeroInteractions(customElementProvider);
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
    }

    @Test
    public void testBindModel_optionalAbsent() {
        CustomBindingRef bindingRef =
                CustomBindingRef.newBuilder().setBindingId("CUSTOM!").setIsOptional(true).build();
        when(frameContext.getCustomElementBindingValue(bindingRef))
                .thenReturn(BindingValue.getDefaultInstance());
        Element model = asElement(CustomElement.newBuilder().setCustomBinding(bindingRef).build());
        adapter.createAdapter(model, frameContext);

        // This should not fail.
        adapter.bindModel(model, frameContext);
        assertThat(adapter.getView().getVisibility()).isEqualTo(View.GONE);
    }

    @Test
    public void testBindModel_noContentInBinding() {
        CustomBindingRef bindingRef = CustomBindingRef.newBuilder().setBindingId("CUSTOM").build();
        when(frameContext.getCustomElementBindingValue(bindingRef))
                .thenReturn(BindingValue.newBuilder().setBindingId("CUSTOM").build());
        Element model = asElement(CustomElement.newBuilder().setCustomBinding(bindingRef).build());
        adapter.createAdapter(model, frameContext);

        assertThatRunnable(() -> adapter.bindModel(model, frameContext))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Custom element binding CUSTOM had no content");
    }

    @Test
    public void testUnbindModel() {
        adapter.createAdapter(DEFAULT_MODEL, frameContext);
        adapter.bindModel(DEFAULT_MODEL, frameContext);

        adapter.unbindModel();

        verify(customElementProvider).releaseCustomView(customView, DUMMY_DATA);
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
    }

    @Test
    public void testUnbindModel_noChildren() {
        adapter.createAdapter(DEFAULT_MODEL, frameContext);

        adapter.unbindModel();

        verifyZeroInteractions(customElementProvider);
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
    }

    @Test
    public void testUnbindModel_multipleChildren() {
        View customView2 = new View(context);
        when(customElementProvider.createCustomElement(DUMMY_DATA))
                .thenReturn(customView)
                .thenReturn(customView2);

        adapter.createAdapter(DEFAULT_MODEL, frameContext);
        adapter.bindModel(DEFAULT_MODEL, frameContext);
        adapter.bindModel(DEFAULT_MODEL, frameContext);

        adapter.unbindModel();

        verify(customElementProvider).releaseCustomView(customView, DUMMY_DATA);
        verify(customElementProvider).releaseCustomView(customView2, DUMMY_DATA);
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
    }

    @Test
    public void testGetModelFromElement() {
        CustomElement model = CustomElement.newBuilder().build();

        Element elementWithModel = Element.newBuilder().setCustomElement(model).build();
        assertThat(adapter.getModelFromElement(elementWithModel)).isSameInstanceAs(model);

        Element elementWithWrongModel =
                Element.newBuilder().setTextElement(TextElement.getDefaultInstance()).build();
        assertThatRunnable(() -> adapter.getModelFromElement(elementWithWrongModel))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Missing CustomElement");

        Element emptyElement = Element.getDefaultInstance();
        assertThatRunnable(() -> adapter.getModelFromElement(emptyElement))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Missing CustomElement");
    }

    private static Element asElement(CustomElement customElement) {
        return Element.newBuilder().setCustomElement(customElement).build();
    }
}
