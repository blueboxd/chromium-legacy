// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.android.libraries.feed.common.testing.RunnableSubject.assertThatRunnable;
import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;

import com.google.android.libraries.feed.common.functional.Suppliers;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.piet.host.AssetProvider;
import com.google.search.now.ui.piet.BindingRefsProto.ElementBindingRef;
import com.google.search.now.ui.piet.BindingRefsProto.TemplateBindingRef;
import com.google.search.now.ui.piet.ElementsProto.BindingContext;
import com.google.search.now.ui.piet.ElementsProto.BindingValue;
import com.google.search.now.ui.piet.ElementsProto.Content;
import com.google.search.now.ui.piet.ElementsProto.Element;
import com.google.search.now.ui.piet.ElementsProto.ElementList;
import com.google.search.now.ui.piet.ElementsProto.GridRow;
import com.google.search.now.ui.piet.ElementsProto.ImageElement;
import com.google.search.now.ui.piet.ElementsProto.TemplateInvocation;
import com.google.search.now.ui.piet.ElementsProto.TextElement;
import com.google.search.now.ui.piet.ElementsProto.Visibility;
import com.google.search.now.ui.piet.ElementsProto.VisibilityState;
import com.google.search.now.ui.piet.ImagesProto.Image;
import com.google.search.now.ui.piet.PietProto.Template;
import com.google.search.now.ui.piet.StylesProto.StyleIdsStack;
import com.google.search.now.ui.piet.TextProto.ParameterizedText;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Tests of the {@link ElementContainerAdapter}. */
@RunWith(RobolectricTestRunner.class)
public class ElementContainerAdapterTest {
    private static final Element DUMMY_BASE_ELEMENT = Element.getDefaultInstance();
    private static final Element DEFAULT_ELEMENT =
            Element.newBuilder().setElementList(ElementList.newBuilder().build()).build();

    private static final Element DEFAULT_BOUND_ELEMENT =
            Element.newBuilder().setGridRow(GridRow.getDefaultInstance()).build();
    private static final String DEFAULT_ELEMENT_BINDING_ID = "bound-grid-element";
    private static final ElementBindingRef DEFAULT_ELEMENT_BINDING =
            ElementBindingRef.newBuilder().setBindingId(DEFAULT_ELEMENT_BINDING_ID).build();
    private static final BindingValue DEFAULT_ELEMENT_BINDING_VALUE =
            BindingValue.newBuilder()
                    .setBindingId(DEFAULT_ELEMENT_BINDING_ID)
                    .setElement(DEFAULT_BOUND_ELEMENT)
                    .build();

    private static final String DEFAULT_TEMPLATE_ID = "inline-text-template";
    private static final Template DEFAULT_TEMPLATE =
            Template.newBuilder()
                    .setTemplateId(DEFAULT_TEMPLATE_ID)
                    .setElement(Element.newBuilder().setTextElement(
                            TextElement.newBuilder().setParameterizedText(
                                    ParameterizedText.getDefaultInstance())))
                    .build();
    private static final TemplateInvocation DEFAULT_TEMPLATE_INVOCATION =
            TemplateInvocation.newBuilder()
                    .setTemplateId(DEFAULT_TEMPLATE_ID)
                    .addBindingContexts(BindingContext.newBuilder().addBindingValues(
                            BindingValue.newBuilder().setBindingId("T1")))
                    .addBindingContexts(BindingContext.newBuilder().addBindingValues(
                            BindingValue.newBuilder().setBindingId("T2")))
                    .build();

    private static final String DEFAULT_BOUND_TEMPLATE_ID = "bound-image-template";
    private static final Template DEFAULT_BOUND_TEMPLATE =
            Template.newBuilder()
                    .setTemplateId(DEFAULT_BOUND_TEMPLATE_ID)
                    .setElement(Element.newBuilder().setImageElement(
                            ImageElement.newBuilder().setImage(Image.getDefaultInstance())))
                    .build();
    private static final TemplateInvocation DEFAULT_BOUND_TEMPLATE_INVOCATION =
            TemplateInvocation.newBuilder()
                    .setTemplateId(DEFAULT_BOUND_TEMPLATE_ID)
                    .addBindingContexts(BindingContext.newBuilder().addBindingValues(
                            BindingValue.newBuilder().setBindingId("BT1")))
                    .addBindingContexts(BindingContext.newBuilder().addBindingValues(
                            BindingValue.newBuilder().setBindingId("BT2")))
                    .addBindingContexts(BindingContext.newBuilder().addBindingValues(
                            BindingValue.newBuilder().setBindingId("BT3")))
                    .build();
    private static final String DEFAULT_TEMPLATE_BINDING_ID = "bind-the-bound-image-template";
    private static final TemplateBindingRef DEFAULT_TEMPLATE_BINDING =
            TemplateBindingRef.newBuilder().setBindingId(DEFAULT_TEMPLATE_BINDING_ID).build();
    private static final BindingValue TEMPLATE_BINDING_VALUE =
            BindingValue.newBuilder()
                    .setBindingId(DEFAULT_TEMPLATE_BINDING_ID)
                    .setTemplateInvocation(DEFAULT_BOUND_TEMPLATE_INVOCATION)
                    .build();

    @Mock
    private FrameContext frameContext;
    @Mock
    private HostProviders hostProviders;
    @Mock
    private AssetProvider assetProvider;

    private Context context;

    private TestElementContainerAdapter adapter;

    @Before
    public void setUp() {
        initMocks(this);
        context = Robolectric.buildActivity(Activity.class).get();

        when(hostProviders.getAssetProvider()).thenReturn(assetProvider);
        AdapterParameters adapterParameters = new AdapterParameters(
                context, Suppliers.of(null), hostProviders, new FakeClock(), false, false);

        when(frameContext.createTemplateContext(eq(DEFAULT_TEMPLATE), any(BindingContext.class)))
                .thenReturn(frameContext);
        when(frameContext.createTemplateContext(
                     eq(DEFAULT_BOUND_TEMPLATE), any(BindingContext.class)))
                .thenReturn(frameContext);
        when(frameContext.makeStyleFor(any(StyleIdsStack.class)))
                .thenReturn(adapterParameters.defaultStyleProvider);
        when(frameContext.filterImageSourcesByMediaQueryCondition(any(Image.class)))
                .thenAnswer(invocation -> invocation.getArguments()[0]);

        // Default no-errors behavior
        when(frameContext.getTemplateInvocationBindingValue(DEFAULT_TEMPLATE_BINDING))
                .thenReturn(TEMPLATE_BINDING_VALUE);
        when(frameContext.getTemplate(DEFAULT_TEMPLATE_ID)).thenReturn(DEFAULT_TEMPLATE);
        when(frameContext.getTemplate(DEFAULT_BOUND_TEMPLATE_ID))
                .thenReturn(DEFAULT_BOUND_TEMPLATE);
        when(frameContext.getElementBindingValue(DEFAULT_ELEMENT_BINDING))
                .thenReturn(DEFAULT_ELEMENT_BINDING_VALUE);

        LinearLayout view = new LinearLayout(context);

        adapter = new TestElementContainerAdapter(context, adapterParameters, view);
    }

    @Test
    public void testCreateChildAdapters_elementCreatesOneAdapter() {
        adapter.createAdapter(
                Collections.singletonList(Content.newBuilder().setElement(DEFAULT_ELEMENT).build()),
                DUMMY_BASE_ELEMENT, frameContext);

        // A single adapter created from the Element.
        assertThat(adapter.childAdapters).hasSize(1);
        assertThat(adapter.childAdapters.get(0)).isInstanceOf(ElementListAdapter.class);
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(1);
        assertThat(adapter.getBaseView().getChildAt(0))
                .isSameInstanceAs(adapter.childAdapters.get(0).getView());
        assertThat(adapter.adaptersPerContent).isEqualTo(new int[] {1});
    }

    @Test
    public void testCreateChildAdapters_templateCreatesMultipleAdapters() {
        adapter.createAdapter(Collections.singletonList(
                                      Content.newBuilder()
                                              .setTemplateInvocation(DEFAULT_TEMPLATE_INVOCATION)
                                              .build()),
                DUMMY_BASE_ELEMENT, frameContext);

        // One TemplateInstanceAdapter for each BindingContext in the TemplateInvocation.
        assertThat(DEFAULT_TEMPLATE_INVOCATION.getBindingContextsCount()).isEqualTo(2);
        assertThat(adapter.childAdapters).hasSize(2);
        assertThat(adapter.childAdapters.get(0)).isInstanceOf(TextElementAdapter.class);
        assertThat(adapter.childAdapters.get(1)).isInstanceOf(TextElementAdapter.class);
        verify(frameContext)
                .createTemplateContext(
                        DEFAULT_TEMPLATE, DEFAULT_TEMPLATE_INVOCATION.getBindingContexts(0));
        verify(frameContext)
                .createTemplateContext(
                        DEFAULT_TEMPLATE, DEFAULT_TEMPLATE_INVOCATION.getBindingContexts(1));

        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(2);
        assertThat(adapter.getBaseView().getChildAt(0))
                .isSameInstanceAs(adapter.childAdapters.get(0).getView());
        assertThat(adapter.getBaseView().getChildAt(1))
                .isSameInstanceAs(adapter.childAdapters.get(1).getView());
        assertThat(adapter.adaptersPerContent).isEqualTo(new int[] {2});
    }

    @Test
    public void testCreateChildAdapters_templateNotFound() {
        when(frameContext.getTemplate(DEFAULT_TEMPLATE_ID)).thenReturn(null);

        adapter.createAdapter(Collections.singletonList(
                                      Content.newBuilder()
                                              .setTemplateInvocation(DEFAULT_TEMPLATE_INVOCATION)
                                              .build()),
                DUMMY_BASE_ELEMENT, frameContext);

        // No child adapters are created.
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(adapter.childAdapters).isEmpty();
        assertThat(adapter.adaptersPerContent).isEqualTo(new int[] {0});
    }

    @Test
    public void testCreateChildAdapters_boundElement_doesNothing() {
        adapter.createAdapter(
                Collections.singletonList(
                        Content.newBuilder().setBoundElement(DEFAULT_ELEMENT_BINDING).build()),
                DUMMY_BASE_ELEMENT, frameContext);

        // No child adapters are created.
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(adapter.childAdapters).isEmpty();
        assertThat(adapter.adaptersPerContent).isEqualTo(new int[] {0});
    }

    @Test
    public void testCreateChildAdapters_boundElementNotFound_doesNothing() {
        when(frameContext.getElementBindingValue(DEFAULT_ELEMENT_BINDING))
                .thenReturn(BindingValue.getDefaultInstance());

        adapter.createAdapter(
                Collections.singletonList(
                        Content.newBuilder().setBoundElement(DEFAULT_ELEMENT_BINDING).build()),
                DUMMY_BASE_ELEMENT, frameContext);

        // No child adapters are created.
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(adapter.childAdapters).isEmpty();
        assertThat(adapter.adaptersPerContent).isEqualTo(new int[] {0});
    }

    @Test
    public void testCreateChildAdapters_boundTemplate_doesNothing() {
        adapter.createAdapter(
                Collections.singletonList(
                        Content.newBuilder().setTemplateBinding(DEFAULT_TEMPLATE_BINDING).build()),
                DUMMY_BASE_ELEMENT, frameContext);

        // No child adapters are created.
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(adapter.childAdapters).isEmpty();
        assertThat(adapter.adaptersPerContent).isEqualTo(new int[] {0});
    }

    @Test
    public void testCreateChildAdapters_boundTemplateNotFound_doesNothing() {
        when(frameContext.getTemplateInvocationBindingValue(DEFAULT_TEMPLATE_BINDING))
                .thenReturn(BindingValue.getDefaultInstance());

        adapter.createAdapter(
                Collections.singletonList(
                        Content.newBuilder().setTemplateBinding(DEFAULT_TEMPLATE_BINDING).build()),
                DUMMY_BASE_ELEMENT, frameContext);

        // No child adapters are created.
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(adapter.childAdapters).isEmpty();
        assertThat(adapter.adaptersPerContent).isEqualTo(new int[] {0});
    }

    @Test
    public void testCreateChildAdapters_boundTemplateBetweenInlineContents() {
        List<Content> contents = new ArrayList<>();
        contents.add(
                Content.newBuilder().setTemplateInvocation(DEFAULT_TEMPLATE_INVOCATION).build());
        contents.add(Content.newBuilder().setTemplateBinding(DEFAULT_TEMPLATE_BINDING).build());
        contents.add(Content.newBuilder().setElement(DEFAULT_ELEMENT).build());

        adapter.createAdapter(contents, DUMMY_BASE_ELEMENT, frameContext);

        // Adapters created:  2 templates, (nothing for the binding), 1 element list
        assertThat(adapter.childAdapters).hasSize(3);
        assertThat(adapter.childAdapters.get(0)).isInstanceOf(TextElementAdapter.class);
        assertThat(adapter.childAdapters.get(1)).isInstanceOf(TextElementAdapter.class);

        assertThat(adapter.childAdapters.get(2)).isInstanceOf(ElementListAdapter.class);

        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(3);
        for (int i = 0; i < 3; i++) {
            assertThat(adapter.getBaseView().getChildAt(i))
                    .isSameInstanceAs(adapter.childAdapters.get(i).getView());
        }

        assertThat(adapter.adaptersPerContent).isEqualTo(new int[] {2, 0, 1});
    }

    @Test
    public void testCreateChildAdapters_failsIfAdapterNotReleased() {
        adapter.createAdapter(
                Collections.singletonList(Content.newBuilder().setElement(DEFAULT_ELEMENT).build()),
                DUMMY_BASE_ELEMENT, frameContext);

        assertThatRunnable(
                ()
                        -> adapter.createAdapter(
                                Collections.singletonList(
                                        Content.newBuilder().setElement(DEFAULT_ELEMENT).build()),
                                DUMMY_BASE_ELEMENT, frameContext))
                .throwsAnExceptionOfType(IllegalStateException.class)
                .that()
                .hasMessageThat()
                .contains("release adapter before creating");
    }

    @Test
    public void testBindChildAdapters_createsAdapterIfNotAlreadyCreated() {
        adapter.bindModel(
                Collections.singletonList(Content.newBuilder().setElement(DEFAULT_ELEMENT).build()),
                DUMMY_BASE_ELEMENT, frameContext);

        assertThat(adapter.childAdapters).hasSize(1);
        assertThat(adapter.childAdapters.get(0)).isInstanceOf(ElementListAdapter.class);
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(1);
    }

    @Test
    public void testBindChildAdapters_element() {
        List<Content> contents =
                Collections.singletonList(Content.newBuilder().setElement(DEFAULT_ELEMENT).build());
        adapter.createAdapter(contents, DUMMY_BASE_ELEMENT, frameContext);
        ElementAdapter<? extends View, ?> mockChildAdapter = mock(ElementListAdapter.class);
        adapter.childAdapters.set(0, mockChildAdapter);

        adapter.bindModel(contents, DUMMY_BASE_ELEMENT, frameContext);

        verify(mockChildAdapter).bindModel(DEFAULT_ELEMENT, frameContext);
    }

    @Test
    public void testBindChildAdapters_template() {
        List<Content> contents = Collections.singletonList(
                Content.newBuilder().setTemplateInvocation(DEFAULT_TEMPLATE_INVOCATION).build());
        adapter.createAdapter(contents, DUMMY_BASE_ELEMENT, frameContext);
        RecyclerKey key1 = adapter.childAdapters.get(0).getKey();
        RecyclerKey key2 = adapter.childAdapters.get(1).getKey();
        TextElementAdapter mockChildAdapter1 = mock(TextElementAdapter.class);
        TextElementAdapter mockChildAdapter2 = mock(TextElementAdapter.class);
        when(mockChildAdapter1.getKey()).thenReturn(key1);
        when(mockChildAdapter2.getKey()).thenReturn(key2);
        adapter.childAdapters.set(0, mockChildAdapter1);
        adapter.childAdapters.set(1, mockChildAdapter2);

        FrameContext templateContext1 = mock(FrameContext.class);
        FrameContext templateContext2 = mock(FrameContext.class);
        when(frameContext.createTemplateContext(
                     DEFAULT_TEMPLATE, DEFAULT_TEMPLATE_INVOCATION.getBindingContexts(0)))
                .thenReturn(templateContext1);
        when(frameContext.createTemplateContext(
                     DEFAULT_TEMPLATE, DEFAULT_TEMPLATE_INVOCATION.getBindingContexts(1)))
                .thenReturn(templateContext2);

        adapter.bindModel(contents, DUMMY_BASE_ELEMENT, frameContext);

        verify(mockChildAdapter1).bindModel(DEFAULT_TEMPLATE.getElement(), templateContext1);
        verify(mockChildAdapter2).bindModel(DEFAULT_TEMPLATE.getElement(), templateContext2);
    }

    @Test
    public void testBindChildAdapters_templateNotFound() {
        when(frameContext.getTemplate(DEFAULT_TEMPLATE_ID)).thenReturn(null);

        List<Content> contents = Collections.singletonList(
                Content.newBuilder().setTemplateInvocation(DEFAULT_TEMPLATE_INVOCATION).build());
        adapter.createAdapter(contents, DUMMY_BASE_ELEMENT, frameContext);

        adapter.bindModel(contents, DUMMY_BASE_ELEMENT, frameContext);

        // No child adapters are created; binding does nothing.
        assertThat(adapter.childAdapters).isEmpty();
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(adapter.adaptersPerContent).isEqualTo(new int[] {0});
    }

    @Test
    public void testBindChildAdapters_boundElement() {
        List<Content> contents = Collections.singletonList(
                Content.newBuilder().setBoundElement(DEFAULT_ELEMENT_BINDING).build());
        adapter.createAdapter(contents, DUMMY_BASE_ELEMENT, frameContext);

        adapter.bindModel(contents, DUMMY_BASE_ELEMENT, frameContext);

        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(1);
        assertThat(adapter.getBaseView().getChildAt(0))
                .isSameInstanceAs(adapter.childAdapters.get(0).getView());

        assertThat(adapter.adaptersPerContent).isEqualTo(new int[] {1});
    }

    @Test
    public void testBindChildAdapters_boundTemplate() {
        List<Content> contents = Collections.singletonList(
                Content.newBuilder().setTemplateBinding(DEFAULT_TEMPLATE_BINDING).build());
        adapter.createAdapter(contents, DUMMY_BASE_ELEMENT, frameContext);

        adapter.bindModel(contents, DUMMY_BASE_ELEMENT, frameContext);

        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(3);
        assertThat(adapter.getBaseView().getChildAt(0))
                .isSameInstanceAs(adapter.childAdapters.get(0).getView());
        assertThat(adapter.getBaseView().getChildAt(1))
                .isSameInstanceAs(adapter.childAdapters.get(1).getView());
        assertThat(adapter.getBaseView().getChildAt(2))
                .isSameInstanceAs(adapter.childAdapters.get(2).getView());

        assertThat(adapter.adaptersPerContent).isEqualTo(new int[] {3});
    }

    @Test
    public void testBindChildAdapters_boundTemplateNotFound_doesNothing() {
        when(frameContext.getTemplate(DEFAULT_BOUND_TEMPLATE_ID)).thenReturn(null);

        List<Content> contents = Collections.singletonList(
                Content.newBuilder().setTemplateBinding(DEFAULT_TEMPLATE_BINDING).build());
        adapter.createAdapter(contents, DUMMY_BASE_ELEMENT, frameContext);

        adapter.bindModel(contents, DUMMY_BASE_ELEMENT, frameContext);

        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(adapter.adaptersPerContent).isEqualTo(new int[] {0});
    }

    @Test
    public void testBindChildAdapters_boundTemplateBindingNotFound_doesNothing() {
        when(frameContext.getTemplateInvocationBindingValue(DEFAULT_TEMPLATE_BINDING))
                .thenReturn(BindingValue.getDefaultInstance());

        List<Content> contents = Collections.singletonList(
                Content.newBuilder().setTemplateBinding(DEFAULT_TEMPLATE_BINDING).build());
        adapter.createAdapter(contents, DUMMY_BASE_ELEMENT, frameContext);

        adapter.bindModel(contents, DUMMY_BASE_ELEMENT, frameContext);

        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(adapter.adaptersPerContent).isEqualTo(new int[] {0});
    }

    @Test
    public void testBindChildAdapters_boundTemplateBetweenInlineContents() {
        List<Content> contents = new ArrayList<>();
        contents.add(
                Content.newBuilder().setTemplateInvocation(DEFAULT_TEMPLATE_INVOCATION).build());
        contents.add(Content.newBuilder().setTemplateBinding(DEFAULT_TEMPLATE_BINDING).build());
        contents.add(Content.newBuilder().setElement(DEFAULT_ELEMENT).build());

        adapter.createAdapter(contents, DUMMY_BASE_ELEMENT, frameContext);
        adapter.bindModel(contents, DUMMY_BASE_ELEMENT, frameContext);

        // 2 templates, 3 templates, 1 element list
        assertThat(adapter.childAdapters).hasSize(6);

        assertThat(adapter.childAdapters.get(0)).isInstanceOf(TextElementAdapter.class);
        assertThat(adapter.childAdapters.get(1)).isInstanceOf(TextElementAdapter.class);

        assertThat(adapter.childAdapters.get(2)).isInstanceOf(ImageElementAdapter.class);
        assertThat(adapter.childAdapters.get(3)).isInstanceOf(ImageElementAdapter.class);
        assertThat(adapter.childAdapters.get(4)).isInstanceOf(ImageElementAdapter.class);

        assertThat(adapter.childAdapters.get(5)).isInstanceOf(ElementListAdapter.class);

        // Each of these occur 2 times; once for create and once for bind.
        verify(frameContext, times(2))
                .createTemplateContext(
                        DEFAULT_TEMPLATE, DEFAULT_TEMPLATE_INVOCATION.getBindingContexts(0));
        verify(frameContext, times(2))
                .createTemplateContext(
                        DEFAULT_TEMPLATE, DEFAULT_TEMPLATE_INVOCATION.getBindingContexts(1));

        // These only occur once because we do create and bind at the same time.
        verify(frameContext)
                .createTemplateContext(DEFAULT_BOUND_TEMPLATE,
                        DEFAULT_BOUND_TEMPLATE_INVOCATION.getBindingContexts(0));
        verify(frameContext)
                .createTemplateContext(DEFAULT_BOUND_TEMPLATE,
                        DEFAULT_BOUND_TEMPLATE_INVOCATION.getBindingContexts(1));
        verify(frameContext)
                .createTemplateContext(DEFAULT_BOUND_TEMPLATE,
                        DEFAULT_BOUND_TEMPLATE_INVOCATION.getBindingContexts(2));

        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(6);
        for (int i = 0; i < 6; i++) {
            assertThat(adapter.getBaseView().getChildAt(i))
                    .isSameInstanceAs(adapter.childAdapters.get(i).getView());
        }

        assertThat(adapter.adaptersPerContent).isEqualTo(new int[] {2, 3, 1});
    }

    @Test
    public void testUnbindChildAdapters_inline() {
        List<Content> contents = new ArrayList<>();
        contents.add(
                Content.newBuilder().setTemplateInvocation(DEFAULT_TEMPLATE_INVOCATION).build());
        contents.add(Content.newBuilder().setElement(DEFAULT_ELEMENT).build());

        adapter.createAdapter(contents, DUMMY_BASE_ELEMENT, frameContext);
        adapter.bindModel(contents, DUMMY_BASE_ELEMENT, frameContext);

        adapter.unbindModel();

        // 2 templates, 1 element list
        assertThat(adapter.childAdapters).hasSize(3);
        assertThat(adapter.childAdapters.get(0)).isInstanceOf(TextElementAdapter.class);
        assertThat(adapter.childAdapters.get(1)).isInstanceOf(TextElementAdapter.class);
        assertThat(adapter.childAdapters.get(2)).isInstanceOf(ElementListAdapter.class);

        assertThat(adapter.childAdapters.get(0).getRawModel()).isNull();
        assertThat(adapter.childAdapters.get(1).getRawModel()).isNull();
        assertThat(adapter.childAdapters.get(2).getRawModel()).isNull();

        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(3);
        for (int i = 0; i < 3; i++) {
            assertThat(adapter.getBaseView().getChildAt(i))
                    .isSameInstanceAs(adapter.childAdapters.get(i).getView());
        }

        assertThat(adapter.adaptersPerContent).isEqualTo(new int[] {2, 1});
    }

    @Test
    public void testUnbindChildAdapters_boundElement() {
        List<Content> contents = Collections.singletonList(
                Content.newBuilder().setBoundElement(DEFAULT_ELEMENT_BINDING).build());
        adapter.createAdapter(contents, DUMMY_BASE_ELEMENT, frameContext);
        adapter.bindModel(contents, DUMMY_BASE_ELEMENT, frameContext);

        adapter.unbindModel();

        // All (bound) adapters and views have been destroyed
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(adapter.childAdapters).isEmpty();
        assertThat(adapter.adaptersPerContent).isEqualTo(new int[] {0});
    }

    @Test
    public void testUnbindChildAdapters_boundTemplate() {
        List<Content> contents = Collections.singletonList(
                Content.newBuilder().setTemplateBinding(DEFAULT_TEMPLATE_BINDING).build());
        adapter.createAdapter(contents, DUMMY_BASE_ELEMENT, frameContext);
        adapter.bindModel(contents, DUMMY_BASE_ELEMENT, frameContext);

        adapter.unbindModel();

        // All (bound) adapters and views have been destroyed
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(adapter.childAdapters).isEmpty();
        assertThat(adapter.adaptersPerContent).isEqualTo(new int[] {0});
    }

    @Test
    public void testUnbindChildAdapters_boundContentBetweenInlineContents() {
        List<Content> contents = new ArrayList<>();
        contents.add(
                Content.newBuilder().setTemplateInvocation(DEFAULT_TEMPLATE_INVOCATION).build());
        contents.add(Content.newBuilder().setTemplateBinding(DEFAULT_TEMPLATE_BINDING).build());
        contents.add(Content.newBuilder().setElement(DEFAULT_ELEMENT).build());

        adapter.createAdapter(contents, DUMMY_BASE_ELEMENT, frameContext);
        adapter.bindModel(contents, DUMMY_BASE_ELEMENT, frameContext);

        assertThat(adapter.childAdapters).hasSize(6);
        ImageElementAdapter mockTemplateAdapter = mock(ImageElementAdapter.class);
        adapter.childAdapters.set(3, mockTemplateAdapter);

        adapter.unbindModel();

        // The inline adapters have been unbound; the bound adapters have been destroyed.
        assertThat(adapter.childAdapters).hasSize(3);
        assertThat(adapter.childAdapters.get(0).getRawModel()).isNull();
        assertThat(adapter.childAdapters.get(1).getRawModel()).isNull();
        assertThat(adapter.childAdapters.get(2).getRawModel()).isNull();

        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(3);
        for (int i = 0; i < 3; i++) {
            assertThat(adapter.getBaseView().getChildAt(i))
                    .isSameInstanceAs(adapter.childAdapters.get(i).getView());
        }

        assertThat(adapter.adaptersPerContent).isEqualTo(new int[] {2, 0, 1});

        verify(mockTemplateAdapter).unbindModel();
        verify(mockTemplateAdapter).releaseAdapter();
    }

    @Test
    public void testUnbind_visibilityWasGone() {
        List<Content> contents = new ArrayList<>();
        contents.add(
                Content.newBuilder().setTemplateInvocation(DEFAULT_TEMPLATE_INVOCATION).build());
        contents.add(Content.newBuilder().setElement(DEFAULT_ELEMENT).build());

        Element goneElement =
                Element.newBuilder()
                        .setVisibilityState(
                                VisibilityState.newBuilder().setDefaultVisibility(Visibility.GONE))
                        .build();

        adapter.createAdapter(contents, goneElement, frameContext);
        adapter.bindModel(contents, goneElement, frameContext);

        // Nothing was created because the visibility was GONE
        assertThat(adapter.childAdapters).isEmpty();
        assertThat(adapter.adaptersPerContent).isEmpty();

        adapter.unbindModel();

        // Nothing crashes, and the adapter is still uncreated.
        assertThat(adapter.childAdapters).isEmpty();
        assertThat(adapter.adaptersPerContent).isEmpty();
        assertThat(adapter.getBaseView().getChildCount()).isEqualTo(0);
    }

    private static class TestElementContainerAdapter
            extends ElementContainerAdapter<LinearLayout, List<Content>> {
        TestElementContainerAdapter(
                Context context, AdapterParameters adapterParameters, LinearLayout view) {
            super(context, adapterParameters, view);
        }

        @Override
        List<Content> getContentsFromModel(List<Content> model) {
            return model;
        }

        @Override
        List<Content> getModelFromElement(Element baseElement) {
            throw new UnsupportedOperationException("Not used in this test");
        }
    }
}
