// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.common.functional.Suppliers;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.piet.host.AssetProvider;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ElementBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.TemplateBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingContext;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingValue;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Content;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ElementList;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.GridRow;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ImageElement;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.TemplateInvocation;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.TextElement;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Visibility;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.VisibilityState;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.Image;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Template;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.StyleIdsStack;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.ParameterizedText;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Tests of the {@link ElementContainerAdapter}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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
    private FrameContext mFrameContext;
    @Mock
    private HostProviders mHostProviders;
    @Mock
    private AssetProvider mAssetProvider;

    private Context mContext;

    private TestElementContainerAdapter mAdapter;

    @Before
    public void setUp() {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();

        when(mHostProviders.getAssetProvider()).thenReturn(mAssetProvider);
        AdapterParameters adapterParameters = new AdapterParameters(
                mContext, Suppliers.of(null), mHostProviders, new FakeClock(), false, false);

        when(mFrameContext.createTemplateContext(eq(DEFAULT_TEMPLATE), any(BindingContext.class)))
                .thenReturn(mFrameContext);
        when(mFrameContext.createTemplateContext(
                     eq(DEFAULT_BOUND_TEMPLATE), any(BindingContext.class)))
                .thenReturn(mFrameContext);
        when(mFrameContext.makeStyleFor(any(StyleIdsStack.class)))
                .thenReturn(adapterParameters.mDefaultStyleProvider);
        when(mFrameContext.filterImageSourcesByMediaQueryCondition(any(Image.class)))
                .thenAnswer(invocation -> invocation.getArguments()[0]);

        // Default no-errors behavior
        when(mFrameContext.getTemplateInvocationBindingValue(DEFAULT_TEMPLATE_BINDING))
                .thenReturn(TEMPLATE_BINDING_VALUE);
        when(mFrameContext.getTemplate(DEFAULT_TEMPLATE_ID)).thenReturn(DEFAULT_TEMPLATE);
        when(mFrameContext.getTemplate(DEFAULT_BOUND_TEMPLATE_ID))
                .thenReturn(DEFAULT_BOUND_TEMPLATE);
        when(mFrameContext.getElementBindingValue(DEFAULT_ELEMENT_BINDING))
                .thenReturn(DEFAULT_ELEMENT_BINDING_VALUE);

        LinearLayout view = new LinearLayout(mContext);

        mAdapter = new TestElementContainerAdapter(mContext, adapterParameters, view);
    }

    @Test
    public void testCreateChildAdapters_elementCreatesOneAdapter() {
        mAdapter.createAdapter(
                Collections.singletonList(Content.newBuilder().setElement(DEFAULT_ELEMENT).build()),
                DUMMY_BASE_ELEMENT, mFrameContext);

        // A single adapter created from the Element.
        assertThat(mAdapter.mChildAdapters).hasSize(1);
        assertThat(mAdapter.mChildAdapters.get(0)).isInstanceOf(ElementListAdapter.class);
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(1);
        assertThat(mAdapter.getBaseView().getChildAt(0))
                .isSameInstanceAs(mAdapter.mChildAdapters.get(0).getView());
        assertThat(mAdapter.mAdaptersPerContent).isEqualTo(new int[] {1});
    }

    @Test
    public void testCreateChildAdapters_templateCreatesMultipleAdapters() {
        mAdapter.createAdapter(Collections.singletonList(
                                       Content.newBuilder()
                                               .setTemplateInvocation(DEFAULT_TEMPLATE_INVOCATION)
                                               .build()),
                DUMMY_BASE_ELEMENT, mFrameContext);

        // One TemplateInstanceAdapter for each BindingContext in the TemplateInvocation.
        assertThat(DEFAULT_TEMPLATE_INVOCATION.getBindingContextsCount()).isEqualTo(2);
        assertThat(mAdapter.mChildAdapters).hasSize(2);
        assertThat(mAdapter.mChildAdapters.get(0)).isInstanceOf(TextElementAdapter.class);
        assertThat(mAdapter.mChildAdapters.get(1)).isInstanceOf(TextElementAdapter.class);
        verify(mFrameContext)
                .createTemplateContext(
                        DEFAULT_TEMPLATE, DEFAULT_TEMPLATE_INVOCATION.getBindingContexts(0));
        verify(mFrameContext)
                .createTemplateContext(
                        DEFAULT_TEMPLATE, DEFAULT_TEMPLATE_INVOCATION.getBindingContexts(1));

        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(2);
        assertThat(mAdapter.getBaseView().getChildAt(0))
                .isSameInstanceAs(mAdapter.mChildAdapters.get(0).getView());
        assertThat(mAdapter.getBaseView().getChildAt(1))
                .isSameInstanceAs(mAdapter.mChildAdapters.get(1).getView());
        assertThat(mAdapter.mAdaptersPerContent).isEqualTo(new int[] {2});
    }

    @Test
    public void testCreateChildAdapters_templateNotFound() {
        when(mFrameContext.getTemplate(DEFAULT_TEMPLATE_ID)).thenReturn(null);

        mAdapter.createAdapter(Collections.singletonList(
                                       Content.newBuilder()
                                               .setTemplateInvocation(DEFAULT_TEMPLATE_INVOCATION)
                                               .build()),
                DUMMY_BASE_ELEMENT, mFrameContext);

        // No child adapters are created.
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(mAdapter.mChildAdapters).isEmpty();
        assertThat(mAdapter.mAdaptersPerContent).isEqualTo(new int[] {0});
    }

    @Test
    public void testCreateChildAdapters_boundElement_doesNothing() {
        mAdapter.createAdapter(
                Collections.singletonList(
                        Content.newBuilder().setBoundElement(DEFAULT_ELEMENT_BINDING).build()),
                DUMMY_BASE_ELEMENT, mFrameContext);

        // No child adapters are created.
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(mAdapter.mChildAdapters).isEmpty();
        assertThat(mAdapter.mAdaptersPerContent).isEqualTo(new int[] {0});
    }

    @Test
    public void testCreateChildAdapters_boundElementNotFound_doesNothing() {
        when(mFrameContext.getElementBindingValue(DEFAULT_ELEMENT_BINDING))
                .thenReturn(BindingValue.getDefaultInstance());

        mAdapter.createAdapter(
                Collections.singletonList(
                        Content.newBuilder().setBoundElement(DEFAULT_ELEMENT_BINDING).build()),
                DUMMY_BASE_ELEMENT, mFrameContext);

        // No child adapters are created.
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(mAdapter.mChildAdapters).isEmpty();
        assertThat(mAdapter.mAdaptersPerContent).isEqualTo(new int[] {0});
    }

    @Test
    public void testCreateChildAdapters_boundTemplate_doesNothing() {
        mAdapter.createAdapter(
                Collections.singletonList(
                        Content.newBuilder().setTemplateBinding(DEFAULT_TEMPLATE_BINDING).build()),
                DUMMY_BASE_ELEMENT, mFrameContext);

        // No child adapters are created.
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(mAdapter.mChildAdapters).isEmpty();
        assertThat(mAdapter.mAdaptersPerContent).isEqualTo(new int[] {0});
    }

    @Test
    public void testCreateChildAdapters_boundTemplateNotFound_doesNothing() {
        when(mFrameContext.getTemplateInvocationBindingValue(DEFAULT_TEMPLATE_BINDING))
                .thenReturn(BindingValue.getDefaultInstance());

        mAdapter.createAdapter(
                Collections.singletonList(
                        Content.newBuilder().setTemplateBinding(DEFAULT_TEMPLATE_BINDING).build()),
                DUMMY_BASE_ELEMENT, mFrameContext);

        // No child adapters are created.
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(mAdapter.mChildAdapters).isEmpty();
        assertThat(mAdapter.mAdaptersPerContent).isEqualTo(new int[] {0});
    }

    @Test
    public void testCreateChildAdapters_boundTemplateBetweenInlineContents() {
        List<Content> contents = new ArrayList<>();
        contents.add(
                Content.newBuilder().setTemplateInvocation(DEFAULT_TEMPLATE_INVOCATION).build());
        contents.add(Content.newBuilder().setTemplateBinding(DEFAULT_TEMPLATE_BINDING).build());
        contents.add(Content.newBuilder().setElement(DEFAULT_ELEMENT).build());

        mAdapter.createAdapter(contents, DUMMY_BASE_ELEMENT, mFrameContext);

        // Adapters created:  2 templates, (nothing for the binding), 1 element list
        assertThat(mAdapter.mChildAdapters).hasSize(3);
        assertThat(mAdapter.mChildAdapters.get(0)).isInstanceOf(TextElementAdapter.class);
        assertThat(mAdapter.mChildAdapters.get(1)).isInstanceOf(TextElementAdapter.class);

        assertThat(mAdapter.mChildAdapters.get(2)).isInstanceOf(ElementListAdapter.class);

        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(3);
        for (int i = 0; i < 3; i++) {
            assertThat(mAdapter.getBaseView().getChildAt(i))
                    .isSameInstanceAs(mAdapter.mChildAdapters.get(i).getView());
        }

        assertThat(mAdapter.mAdaptersPerContent).isEqualTo(new int[] {2, 0, 1});
    }

    @Test
    public void testCreateChildAdapters_failsIfAdapterNotReleased() {
        mAdapter.createAdapter(
                Collections.singletonList(Content.newBuilder().setElement(DEFAULT_ELEMENT).build()),
                DUMMY_BASE_ELEMENT, mFrameContext);

        assertThatRunnable(
                ()
                        -> mAdapter.createAdapter(
                                Collections.singletonList(
                                        Content.newBuilder().setElement(DEFAULT_ELEMENT).build()),
                                DUMMY_BASE_ELEMENT, mFrameContext))
                .throwsAnExceptionOfType(IllegalStateException.class)
                .that()
                .hasMessageThat()
                .contains("release adapter before creating");
    }

    @Test
    public void testBindChildAdapters_createsAdapterIfNotAlreadyCreated() {
        mAdapter.bindModel(
                Collections.singletonList(Content.newBuilder().setElement(DEFAULT_ELEMENT).build()),
                DUMMY_BASE_ELEMENT, mFrameContext);

        assertThat(mAdapter.mChildAdapters).hasSize(1);
        assertThat(mAdapter.mChildAdapters.get(0)).isInstanceOf(ElementListAdapter.class);
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(1);
    }

    @Test
    public void testBindChildAdapters_element() {
        List<Content> contents =
                Collections.singletonList(Content.newBuilder().setElement(DEFAULT_ELEMENT).build());
        mAdapter.createAdapter(contents, DUMMY_BASE_ELEMENT, mFrameContext);
        ElementAdapter<? extends View, ?> mockChildAdapter = mock(ElementListAdapter.class);
        mAdapter.mChildAdapters.set(0, mockChildAdapter);

        mAdapter.bindModel(contents, DUMMY_BASE_ELEMENT, mFrameContext);

        verify(mockChildAdapter).bindModel(DEFAULT_ELEMENT, mFrameContext);
    }

    @Test
    public void testBindChildAdapters_template() {
        List<Content> contents = Collections.singletonList(
                Content.newBuilder().setTemplateInvocation(DEFAULT_TEMPLATE_INVOCATION).build());
        mAdapter.createAdapter(contents, DUMMY_BASE_ELEMENT, mFrameContext);
        RecyclerKey key1 = mAdapter.mChildAdapters.get(0).getKey();
        RecyclerKey key2 = mAdapter.mChildAdapters.get(1).getKey();
        TextElementAdapter mockChildAdapter1 = mock(TextElementAdapter.class);
        TextElementAdapter mockChildAdapter2 = mock(TextElementAdapter.class);
        when(mockChildAdapter1.getKey()).thenReturn(key1);
        when(mockChildAdapter2.getKey()).thenReturn(key2);
        mAdapter.mChildAdapters.set(0, mockChildAdapter1);
        mAdapter.mChildAdapters.set(1, mockChildAdapter2);

        FrameContext templateContext1 = mock(FrameContext.class);
        FrameContext templateContext2 = mock(FrameContext.class);
        when(mFrameContext.createTemplateContext(
                     DEFAULT_TEMPLATE, DEFAULT_TEMPLATE_INVOCATION.getBindingContexts(0)))
                .thenReturn(templateContext1);
        when(mFrameContext.createTemplateContext(
                     DEFAULT_TEMPLATE, DEFAULT_TEMPLATE_INVOCATION.getBindingContexts(1)))
                .thenReturn(templateContext2);

        mAdapter.bindModel(contents, DUMMY_BASE_ELEMENT, mFrameContext);

        verify(mockChildAdapter1).bindModel(DEFAULT_TEMPLATE.getElement(), templateContext1);
        verify(mockChildAdapter2).bindModel(DEFAULT_TEMPLATE.getElement(), templateContext2);
    }

    @Test
    public void testBindChildAdapters_templateNotFound() {
        when(mFrameContext.getTemplate(DEFAULT_TEMPLATE_ID)).thenReturn(null);

        List<Content> contents = Collections.singletonList(
                Content.newBuilder().setTemplateInvocation(DEFAULT_TEMPLATE_INVOCATION).build());
        mAdapter.createAdapter(contents, DUMMY_BASE_ELEMENT, mFrameContext);

        mAdapter.bindModel(contents, DUMMY_BASE_ELEMENT, mFrameContext);

        // No child adapters are created; binding does nothing.
        assertThat(mAdapter.mChildAdapters).isEmpty();
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(mAdapter.mAdaptersPerContent).isEqualTo(new int[] {0});
    }

    @Test
    public void testBindChildAdapters_boundElement() {
        List<Content> contents = Collections.singletonList(
                Content.newBuilder().setBoundElement(DEFAULT_ELEMENT_BINDING).build());
        mAdapter.createAdapter(contents, DUMMY_BASE_ELEMENT, mFrameContext);

        mAdapter.bindModel(contents, DUMMY_BASE_ELEMENT, mFrameContext);

        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(1);
        assertThat(mAdapter.getBaseView().getChildAt(0))
                .isSameInstanceAs(mAdapter.mChildAdapters.get(0).getView());

        assertThat(mAdapter.mAdaptersPerContent).isEqualTo(new int[] {1});
    }

    @Test
    public void testBindChildAdapters_boundTemplate() {
        List<Content> contents = Collections.singletonList(
                Content.newBuilder().setTemplateBinding(DEFAULT_TEMPLATE_BINDING).build());
        mAdapter.createAdapter(contents, DUMMY_BASE_ELEMENT, mFrameContext);

        mAdapter.bindModel(contents, DUMMY_BASE_ELEMENT, mFrameContext);

        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(3);
        assertThat(mAdapter.getBaseView().getChildAt(0))
                .isSameInstanceAs(mAdapter.mChildAdapters.get(0).getView());
        assertThat(mAdapter.getBaseView().getChildAt(1))
                .isSameInstanceAs(mAdapter.mChildAdapters.get(1).getView());
        assertThat(mAdapter.getBaseView().getChildAt(2))
                .isSameInstanceAs(mAdapter.mChildAdapters.get(2).getView());

        assertThat(mAdapter.mAdaptersPerContent).isEqualTo(new int[] {3});
    }

    @Test
    public void testBindChildAdapters_boundTemplateNotFound_doesNothing() {
        when(mFrameContext.getTemplate(DEFAULT_BOUND_TEMPLATE_ID)).thenReturn(null);

        List<Content> contents = Collections.singletonList(
                Content.newBuilder().setTemplateBinding(DEFAULT_TEMPLATE_BINDING).build());
        mAdapter.createAdapter(contents, DUMMY_BASE_ELEMENT, mFrameContext);

        mAdapter.bindModel(contents, DUMMY_BASE_ELEMENT, mFrameContext);

        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(mAdapter.mAdaptersPerContent).isEqualTo(new int[] {0});
    }

    @Test
    public void testBindChildAdapters_boundTemplateBindingNotFound_doesNothing() {
        when(mFrameContext.getTemplateInvocationBindingValue(DEFAULT_TEMPLATE_BINDING))
                .thenReturn(BindingValue.getDefaultInstance());

        List<Content> contents = Collections.singletonList(
                Content.newBuilder().setTemplateBinding(DEFAULT_TEMPLATE_BINDING).build());
        mAdapter.createAdapter(contents, DUMMY_BASE_ELEMENT, mFrameContext);

        mAdapter.bindModel(contents, DUMMY_BASE_ELEMENT, mFrameContext);

        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(mAdapter.mAdaptersPerContent).isEqualTo(new int[] {0});
    }

    @Test
    public void testBindChildAdapters_boundTemplateBetweenInlineContents() {
        List<Content> contents = new ArrayList<>();
        contents.add(
                Content.newBuilder().setTemplateInvocation(DEFAULT_TEMPLATE_INVOCATION).build());
        contents.add(Content.newBuilder().setTemplateBinding(DEFAULT_TEMPLATE_BINDING).build());
        contents.add(Content.newBuilder().setElement(DEFAULT_ELEMENT).build());

        mAdapter.createAdapter(contents, DUMMY_BASE_ELEMENT, mFrameContext);
        mAdapter.bindModel(contents, DUMMY_BASE_ELEMENT, mFrameContext);

        // 2 templates, 3 templates, 1 element list
        assertThat(mAdapter.mChildAdapters).hasSize(6);

        assertThat(mAdapter.mChildAdapters.get(0)).isInstanceOf(TextElementAdapter.class);
        assertThat(mAdapter.mChildAdapters.get(1)).isInstanceOf(TextElementAdapter.class);

        assertThat(mAdapter.mChildAdapters.get(2)).isInstanceOf(ImageElementAdapter.class);
        assertThat(mAdapter.mChildAdapters.get(3)).isInstanceOf(ImageElementAdapter.class);
        assertThat(mAdapter.mChildAdapters.get(4)).isInstanceOf(ImageElementAdapter.class);

        assertThat(mAdapter.mChildAdapters.get(5)).isInstanceOf(ElementListAdapter.class);

        // Each of these occur 2 times; once for create and once for bind.
        verify(mFrameContext, times(2))
                .createTemplateContext(
                        DEFAULT_TEMPLATE, DEFAULT_TEMPLATE_INVOCATION.getBindingContexts(0));
        verify(mFrameContext, times(2))
                .createTemplateContext(
                        DEFAULT_TEMPLATE, DEFAULT_TEMPLATE_INVOCATION.getBindingContexts(1));

        // These only occur once because we do create and bind at the same time.
        verify(mFrameContext)
                .createTemplateContext(DEFAULT_BOUND_TEMPLATE,
                        DEFAULT_BOUND_TEMPLATE_INVOCATION.getBindingContexts(0));
        verify(mFrameContext)
                .createTemplateContext(DEFAULT_BOUND_TEMPLATE,
                        DEFAULT_BOUND_TEMPLATE_INVOCATION.getBindingContexts(1));
        verify(mFrameContext)
                .createTemplateContext(DEFAULT_BOUND_TEMPLATE,
                        DEFAULT_BOUND_TEMPLATE_INVOCATION.getBindingContexts(2));

        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(6);
        for (int i = 0; i < 6; i++) {
            assertThat(mAdapter.getBaseView().getChildAt(i))
                    .isSameInstanceAs(mAdapter.mChildAdapters.get(i).getView());
        }

        assertThat(mAdapter.mAdaptersPerContent).isEqualTo(new int[] {2, 3, 1});
    }

    @Test
    public void testUnbindChildAdapters_inline() {
        List<Content> contents = new ArrayList<>();
        contents.add(
                Content.newBuilder().setTemplateInvocation(DEFAULT_TEMPLATE_INVOCATION).build());
        contents.add(Content.newBuilder().setElement(DEFAULT_ELEMENT).build());

        mAdapter.createAdapter(contents, DUMMY_BASE_ELEMENT, mFrameContext);
        mAdapter.bindModel(contents, DUMMY_BASE_ELEMENT, mFrameContext);

        mAdapter.unbindModel();

        // 2 templates, 1 element list
        assertThat(mAdapter.mChildAdapters).hasSize(3);
        assertThat(mAdapter.mChildAdapters.get(0)).isInstanceOf(TextElementAdapter.class);
        assertThat(mAdapter.mChildAdapters.get(1)).isInstanceOf(TextElementAdapter.class);
        assertThat(mAdapter.mChildAdapters.get(2)).isInstanceOf(ElementListAdapter.class);

        assertThat(mAdapter.mChildAdapters.get(0).getRawModel()).isNull();
        assertThat(mAdapter.mChildAdapters.get(1).getRawModel()).isNull();
        assertThat(mAdapter.mChildAdapters.get(2).getRawModel()).isNull();

        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(3);
        for (int i = 0; i < 3; i++) {
            assertThat(mAdapter.getBaseView().getChildAt(i))
                    .isSameInstanceAs(mAdapter.mChildAdapters.get(i).getView());
        }

        assertThat(mAdapter.mAdaptersPerContent).isEqualTo(new int[] {2, 1});
    }

    @Test
    public void testUnbindChildAdapters_boundElement() {
        List<Content> contents = Collections.singletonList(
                Content.newBuilder().setBoundElement(DEFAULT_ELEMENT_BINDING).build());
        mAdapter.createAdapter(contents, DUMMY_BASE_ELEMENT, mFrameContext);
        mAdapter.bindModel(contents, DUMMY_BASE_ELEMENT, mFrameContext);

        mAdapter.unbindModel();

        // All (bound) adapters and views have been destroyed
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(mAdapter.mChildAdapters).isEmpty();
        assertThat(mAdapter.mAdaptersPerContent).isEqualTo(new int[] {0});
    }

    @Test
    public void testUnbindChildAdapters_boundTemplate() {
        List<Content> contents = Collections.singletonList(
                Content.newBuilder().setTemplateBinding(DEFAULT_TEMPLATE_BINDING).build());
        mAdapter.createAdapter(contents, DUMMY_BASE_ELEMENT, mFrameContext);
        mAdapter.bindModel(contents, DUMMY_BASE_ELEMENT, mFrameContext);

        mAdapter.unbindModel();

        // All (bound) adapters and views have been destroyed
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(mAdapter.mChildAdapters).isEmpty();
        assertThat(mAdapter.mAdaptersPerContent).isEqualTo(new int[] {0});
    }

    @Test
    public void testUnbindChildAdapters_boundContentBetweenInlineContents() {
        List<Content> contents = new ArrayList<>();
        contents.add(
                Content.newBuilder().setTemplateInvocation(DEFAULT_TEMPLATE_INVOCATION).build());
        contents.add(Content.newBuilder().setTemplateBinding(DEFAULT_TEMPLATE_BINDING).build());
        contents.add(Content.newBuilder().setElement(DEFAULT_ELEMENT).build());

        mAdapter.createAdapter(contents, DUMMY_BASE_ELEMENT, mFrameContext);
        mAdapter.bindModel(contents, DUMMY_BASE_ELEMENT, mFrameContext);

        assertThat(mAdapter.mChildAdapters).hasSize(6);
        ImageElementAdapter mockTemplateAdapter = mock(ImageElementAdapter.class);
        mAdapter.mChildAdapters.set(3, mockTemplateAdapter);

        mAdapter.unbindModel();

        // The inline adapters have been unbound; the bound adapters have been destroyed.
        assertThat(mAdapter.mChildAdapters).hasSize(3);
        assertThat(mAdapter.mChildAdapters.get(0).getRawModel()).isNull();
        assertThat(mAdapter.mChildAdapters.get(1).getRawModel()).isNull();
        assertThat(mAdapter.mChildAdapters.get(2).getRawModel()).isNull();

        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(3);
        for (int i = 0; i < 3; i++) {
            assertThat(mAdapter.getBaseView().getChildAt(i))
                    .isSameInstanceAs(mAdapter.mChildAdapters.get(i).getView());
        }

        assertThat(mAdapter.mAdaptersPerContent).isEqualTo(new int[] {2, 0, 1});

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

        mAdapter.createAdapter(contents, goneElement, mFrameContext);
        mAdapter.bindModel(contents, goneElement, mFrameContext);

        // Nothing was created because the visibility was GONE
        assertThat(mAdapter.mChildAdapters).isEmpty();
        assertThat(mAdapter.mAdaptersPerContent).isEmpty();

        mAdapter.unbindModel();

        // Nothing crashes, and the adapter is still uncreated.
        assertThat(mAdapter.mChildAdapters).isEmpty();
        assertThat(mAdapter.mAdaptersPerContent).isEmpty();
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
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
