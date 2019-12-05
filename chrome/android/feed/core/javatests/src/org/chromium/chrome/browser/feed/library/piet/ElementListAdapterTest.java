// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;

import android.app.Activity;
import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;

import org.chromium.chrome.browser.feed.library.common.functional.Suppliers;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.piet.ElementListAdapter.KeySupplier;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler;
import org.chromium.chrome.browser.feed.library.piet.host.AssetProvider;
import org.chromium.chrome.browser.feed.library.testing.shadows.ExtendedShadowLinearLayout;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ElementBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.StyleBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingValue;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Content;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.CustomElement;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ElementList;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ElementStack;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ImageElement;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Visibility;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Frame;
import org.chromium.components.feed.core.proto.ui.piet.RoundedCornersProto.RoundedCorners;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.EdgeWidths;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.StyleIdsStack;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link ElementListAdapter}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ElementListAdapterTest {
    private static final String LIST_STYLE_ID = "manycats";
    private static final StyleIdsStack LIST_STYLES =
            StyleIdsStack.newBuilder().addStyleIds(LIST_STYLE_ID).build();

    private static final Element DEFAULT_ELEMENT =
            Element.newBuilder().setElementStack(ElementStack.getDefaultInstance()).build();
    private static final Content DEFAULT_CONTENT =
            Content.newBuilder().setElement(DEFAULT_ELEMENT).build();
    private static final Element IMAGE_ELEMENT =
            Element.newBuilder().setImageElement(ImageElement.getDefaultInstance()).build();
    private static final ElementList DEFAULT_LIST =
            ElementList.newBuilder().addContents(DEFAULT_CONTENT).build();

    private static final ElementBindingRef ELEMENT_BINDING_REF =
            ElementBindingRef.newBuilder().setBindingId("shopping").build();
    private static final Element LIST_WITH_BOUND_LIST = asElement(
            ElementList.newBuilder()
                    .addContents(Content.newBuilder().setBoundElement(ELEMENT_BINDING_REF))
                    .build());

    @Mock
    private ActionHandler mActionHandler;
    @Mock
    private FrameContext mFrameContext;
    @Mock
    private StyleProvider mStyleProvider;
    @Mock
    private HostProviders mHostProviders;
    @Mock
    private AssetProvider mAssetProvider;

    private Context mContext;
    private AdapterParameters mAdapterParameters;

    private ElementListAdapter mAdapter;

    @Before
    public void setUp() {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();

        when(mFrameContext.makeStyleFor(LIST_STYLES)).thenReturn(mStyleProvider);
        when(mStyleProvider.getPadding()).thenReturn(EdgeWidths.getDefaultInstance());
        when(mFrameContext.getActionHandler()).thenReturn(mActionHandler);
        when(mHostProviders.getAssetProvider()).thenReturn(mAssetProvider);
        when(mAssetProvider.isRtL()).thenReturn(false);
        when(mStyleProvider.getRoundedCorners()).thenReturn(RoundedCorners.getDefaultInstance());

        mAdapterParameters = new AdapterParameters(
                mContext, Suppliers.of(null), mHostProviders, new FakeClock(), false, false);

        when(mFrameContext.makeStyleFor(StyleIdsStack.getDefaultInstance()))
                .thenReturn(mAdapterParameters.mDefaultStyleProvider);

        mAdapter = new KeySupplier().getAdapter(mContext, mAdapterParameters);
    }

    @Test
    public void testViewDoesNotClip() {
        assertThat(mAdapter.getBaseView().getClipToPadding()).isFalse();
    }

    @Test
    public void testOnCreateAdapter_makesList() {
        ElementList listWithStyles = ElementList.newBuilder()
                                             .addContents(DEFAULT_CONTENT)
                                             .addContents(DEFAULT_CONTENT)
                                             .addContents(DEFAULT_CONTENT)
                                             .build();

        mAdapter.createAdapter(asElementWithDefaultStyle(listWithStyles), mFrameContext);

        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(3);
        assertThat(mAdapter.getBaseView().getOrientation()).isEqualTo(LinearLayout.VERTICAL);
    }

    @Test
    public void testOnCreateAdapter_setsStyles() {
        ElementList listWithStyles = ElementList.newBuilder().addContents(DEFAULT_CONTENT).build();

        mAdapter.createAdapter(asElementWithDefaultStyle(listWithStyles), mFrameContext);

        verify(mStyleProvider).applyElementStyles(mAdapter);
    }

    @Config(shadows = {ExtendedShadowLinearLayout.class})
    @Test
    public void testTriggerActions_triggersChildren() {
        Frame frame = Frame.newBuilder().setTag("Frame").build();
        when(mFrameContext.getFrame()).thenReturn(frame);
        Element baseElement =
                Element.newBuilder()
                        .setElementList(ElementList.newBuilder().addContents(
                                Content.newBuilder().setElement(Element.newBuilder().setElementList(
                                        ElementList.getDefaultInstance()))))
                        .build();
        mAdapter.createAdapter(baseElement, mFrameContext);
        mAdapter.bindModel(baseElement, mFrameContext);

        // Replace the child adapter so we can verify on it
        ElementAdapter<?, ?> mockChildAdapter = mock(ElementAdapter.class);
        mAdapter.mChildAdapters.set(0, mockChildAdapter);

        ExtendedShadowLinearLayout shadowView = Shadow.extract(mAdapter.getView());
        mAdapter.getView().setVisibility(View.VISIBLE);
        shadowView.setAttachedToWindow(true);

        View viewport = new View(mContext);

        mAdapter.triggerViewActions(viewport, mFrameContext);

        verify(mockChildAdapter).triggerViewActions(viewport, mFrameContext);
    }

    @Test
    public void testOnBindModel_setsStylesOnlyIfBindingIsDefined() {
        ElementList list = ElementList.newBuilder().addContents(DEFAULT_CONTENT).build();

        mAdapter.createAdapter(asElement(list, LIST_STYLES), mFrameContext);
        verify(mFrameContext).makeStyleFor(LIST_STYLES);

        // Binding an element with a different style will not apply the new style
        StyleIdsStack otherStyles = StyleIdsStack.newBuilder().addStyleIds("bobcat").build();

        mAdapter.bindModel(asElement(list, otherStyles), mFrameContext);
        verify(mFrameContext, never()).makeStyleFor(otherStyles);

        // But binding an element with a style binding will re-apply the style
        StyleIdsStack otherStylesWithBinding =
                StyleIdsStack.newBuilder()
                        .addStyleIds("bobcat")
                        .setStyleBinding(StyleBindingRef.newBuilder().setBindingId("lynx"))
                        .build();
        when(mFrameContext.makeStyleFor(otherStylesWithBinding)).thenReturn(mStyleProvider);

        mAdapter.bindModel(asElement(list, otherStylesWithBinding), mFrameContext);
        verify(mFrameContext).makeStyleFor(otherStylesWithBinding);
    }

    @Test
    public void testOnBindModel_failsWithIncompatibleModel() {
        ElementList listWithThreeElements = ElementList.newBuilder()
                                                    .addContents(DEFAULT_CONTENT)
                                                    .addContents(DEFAULT_CONTENT)
                                                    .addContents(DEFAULT_CONTENT)
                                                    .build();

        mAdapter.createAdapter(asElementWithDefaultStyle(listWithThreeElements), mFrameContext);
        mAdapter.bindModel(asElementWithDefaultStyle(listWithThreeElements), mFrameContext);
        mAdapter.unbindModel();

        ElementList listWithTwoElements = ElementList.newBuilder()
                                                  .addContents(DEFAULT_CONTENT)
                                                  .addContents(DEFAULT_CONTENT)
                                                  .build();

        assertThatRunnable(
                ()
                        -> mAdapter.bindModel(
                                asElementWithDefaultStyle(listWithTwoElements), mFrameContext))
                .throwsAnExceptionOfType(IllegalStateException.class)
                .that()
                .hasMessageThat()
                .contains("Internal error in adapters per content");
    }

    @Test
    public void testOnBindModel_elementListBindingRecreatesAdapter() {
        Element listWithOneItem =
                Element.newBuilder()
                        .setElementList(ElementList.newBuilder().addContents(DEFAULT_CONTENT))
                        .build();
        Element listWithTwoItems = Element.newBuilder()
                                           .setElementList(ElementList.newBuilder()
                                                                   .addContents(DEFAULT_CONTENT)
                                                                   .addContents(DEFAULT_CONTENT))
                                           .build();

        when(mFrameContext.getElementBindingValue(ELEMENT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder().setElement(listWithOneItem).build());
        mAdapter.createAdapter(LIST_WITH_BOUND_LIST, mFrameContext);
        // No child adapters have been created yet.
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(mAdapter.mChildAdapters).isEmpty();

        when(mFrameContext.getElementBindingValue(ELEMENT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder().setElement(listWithTwoItems).build());
        mAdapter.bindModel(LIST_WITH_BOUND_LIST, mFrameContext);
        // The list adapter creates its one view on bind.
        assertThat(((LinearLayout) mAdapter.getBaseView().getChildAt(0)).getChildCount())
                .isEqualTo(2);

        mAdapter.unbindModel();
        // The list adapter has been released.
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(mAdapter.mChildAdapters).isEmpty();

        when(mFrameContext.getElementBindingValue(ELEMENT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder().setElement(listWithOneItem).build());
        mAdapter.bindModel(LIST_WITH_BOUND_LIST, mFrameContext);
        // The list adapter can bind to a different model.
        assertThat(((LinearLayout) mAdapter.getBaseView().getChildAt(0)).getChildCount())
                .isEqualTo(1);
    }

    @Test
    public void testOnBindModel_bindingWithVisibilityGone() {
        when(mFrameContext.getElementBindingValue(ELEMENT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(ELEMENT_BINDING_REF.getBindingId())
                                    .setElement(Element.newBuilder().setElementList(DEFAULT_LIST))
                                    .build());
        mAdapter.createAdapter(LIST_WITH_BOUND_LIST, mFrameContext);
        when(mFrameContext.getElementBindingValue(ELEMENT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(ELEMENT_BINDING_REF.getBindingId())
                                    .setVisibility(Visibility.GONE)
                                    .build());

        mAdapter.bindModel(LIST_WITH_BOUND_LIST, mFrameContext);

        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(mAdapter.mChildAdapters).isEmpty();
    }

    @Test
    public void testOnBindModel_bindingWithNoContent() {
        when(mFrameContext.getElementBindingValue(ELEMENT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(ELEMENT_BINDING_REF.getBindingId())
                                    .setElement(DEFAULT_ELEMENT)
                                    .build());
        mAdapter.createAdapter(LIST_WITH_BOUND_LIST, mFrameContext);
        when(mFrameContext.getElementBindingValue(ELEMENT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(ELEMENT_BINDING_REF.getBindingId())
                                    .build());

        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(mAdapter.mChildAdapters).isEmpty();
    }

    @Test
    public void testOnBindModel_bindingWithOptionalAbsent() {
        when(mFrameContext.getElementBindingValue(ELEMENT_BINDING_REF))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(ELEMENT_BINDING_REF.getBindingId())
                                    .setElement(DEFAULT_ELEMENT)
                                    .build());
        mAdapter.createAdapter(LIST_WITH_BOUND_LIST, mFrameContext);

        ElementBindingRef optionalBinding =
                ELEMENT_BINDING_REF.toBuilder().setIsOptional(true).build();
        ElementList optionalBindingList =
                ElementList.newBuilder()
                        .addContents(Content.newBuilder().setBoundElement(optionalBinding))
                        .build();
        when(mFrameContext.getElementBindingValue(optionalBinding))
                .thenReturn(BindingValue.newBuilder()
                                    .setBindingId(optionalBinding.getBindingId())
                                    .build());

        mAdapter.bindModel(asElement(optionalBindingList), mFrameContext);

        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(mAdapter.mChildAdapters).isEmpty();
    }

    @Test
    public void testOnBindModel_setsLayoutParamsOnChild() {
        ElementList list =
                ElementList.newBuilder()
                        .addContents(Content.newBuilder().setElement(
                                DEFAULT_ELEMENT.toBuilder().setStyleReferences(LIST_STYLES)))
                        .build();
        when(mStyleProvider.getGravityHorizontal(anyInt())).thenReturn(Gravity.CENTER_HORIZONTAL);

        mAdapter.createAdapter(asElement(list), mFrameContext);

        mAdapter.mChildAdapters.get(0).mWidthPx = 123;
        mAdapter.mChildAdapters.get(0).mHeightPx = 456;

        mAdapter.bindModel(asElement(list), mFrameContext);

        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(1);
        assertThat(mAdapter.getBaseView().getChildAt(0).getLayoutParams())
                .isInstanceOf(LinearLayout.LayoutParams.class);
        assertThat(((LayoutParams) mAdapter.getBaseView().getChildAt(0).getLayoutParams()).gravity)
                .isEqualTo(Gravity.CENTER_HORIZONTAL);
        assertThat(((LayoutParams) mAdapter.getBaseView().getChildAt(0).getLayoutParams()).width)
                .isEqualTo(123);
        assertThat(((LayoutParams) mAdapter.getBaseView().getChildAt(0).getLayoutParams()).height)
                .isEqualTo(456);
    }

    @Test
    public void testOnBindModel_setsMargins() {
        String marginsStyleId = "spacecat";
        StyleIdsStack marginsStyles =
                StyleIdsStack.newBuilder().addStyleIds(marginsStyleId).build();
        Element elementWithMargins = Element.newBuilder()
                                             .setStyleReferences(marginsStyles)
                                             .setElementStack(ElementStack.getDefaultInstance())
                                             .build();
        ElementList list = ElementList.newBuilder()
                                   .addContents(Content.newBuilder().setElement(elementWithMargins))
                                   .build();
        when(mFrameContext.makeStyleFor(marginsStyles)).thenReturn(mStyleProvider);

        mAdapter.createAdapter(asElement(list), mFrameContext);
        mAdapter.bindModel(asElement(list), mFrameContext);

        // Assert that applyMargins is called on the child's layout params
        ArgumentCaptor<MarginLayoutParams> capturedLayoutParams =
                ArgumentCaptor.forClass(MarginLayoutParams.class);
        verify(mStyleProvider).applyMargins(eq(mContext), capturedLayoutParams.capture());

        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(1);
        assertThat(mAdapter.getBaseView().getChildAt(0).getLayoutParams())
                .isInstanceOf(MarginLayoutParams.class);
        assertThat(mAdapter.getBaseView().getChildAt(0).getLayoutParams())
                .isSameInstanceAs(capturedLayoutParams.getValue());
    }

    @Test
    public void testReleaseAdapter() {
        ElementList listWithStyles = ElementList.newBuilder()
                                             .addContents(DEFAULT_CONTENT)
                                             .addContents(DEFAULT_CONTENT)
                                             .addContents(DEFAULT_CONTENT)
                                             .build();

        mAdapter.createAdapter(asElementWithDefaultStyle(listWithStyles), mFrameContext);
        mAdapter.bindModel(asElementWithDefaultStyle(listWithStyles), mFrameContext);

        mAdapter.releaseAdapter();
        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(0);
        assertThat(mAdapter.mChildAdapters).isEmpty();
    }

    @Test
    public void testGetVerticalGravity_noModel() {
        assertThat(mAdapter.getVerticalGravity(Gravity.CLIP_VERTICAL))
                .isEqualTo(Gravity.CLIP_VERTICAL);
    }

    @Test
    public void testGetStyleIdsStack() {
        ElementList listWithStyles = ElementList.newBuilder().addContents(DEFAULT_CONTENT).build();

        mAdapter.createAdapter(asElement(listWithStyles, LIST_STYLES), mFrameContext);

        assertThat(mAdapter.getElementStyleIdsStack()).isEqualTo(LIST_STYLES);
    }

    @Test
    public void testGetModelFromElement() {
        ElementList model = ElementList.newBuilder().addContents(DEFAULT_CONTENT).build();

        Element elementWithModel = Element.newBuilder().setElementList(model).build();
        assertThat(mAdapter.getModelFromElement(elementWithModel)).isSameInstanceAs(model);

        Element elementWithWrongModel =
                Element.newBuilder().setCustomElement(CustomElement.getDefaultInstance()).build();
        assertThatRunnable(() -> mAdapter.getModelFromElement(elementWithWrongModel))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Missing ElementList");

        Element emptyElement = Element.getDefaultInstance();
        assertThatRunnable(() -> mAdapter.getModelFromElement(emptyElement))
                .throwsAnExceptionOfType(PietFatalException.class)
                .that()
                .hasMessageThat()
                .contains("Missing ElementList");
    }

    @Test
    public void testSetLayoutParamsOnChild() {
        ElementList list =
                ElementList.newBuilder()
                        .addContents(Content.newBuilder().setElement(
                                DEFAULT_ELEMENT.toBuilder().setStyleReferences(LIST_STYLES)))
                        .build();
        when(mStyleProvider.getWidthSpecPx(mContext)).thenReturn(123);
        when(mStyleProvider.getHeightSpecPx(mContext)).thenReturn(456);
        when(mStyleProvider.getGravityHorizontal(anyInt())).thenReturn(Gravity.CENTER_HORIZONTAL);

        mAdapter.createAdapter(asElement(list), mFrameContext);

        LayoutParams layoutParams = new LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        mAdapter.setLayoutParams(layoutParams);

        assertThat(((LayoutParams) mAdapter.getBaseView().getChildAt(0).getLayoutParams()).gravity)
                .isEqualTo(Gravity.CENTER_HORIZONTAL);
        assertThat(((LayoutParams) mAdapter.getBaseView().getChildAt(0).getLayoutParams()).width)
                .isEqualTo(123);
        assertThat(((LayoutParams) mAdapter.getBaseView().getChildAt(0).getLayoutParams()).height)
                .isEqualTo(456);
    }

    @Test
    public void testSetLayoutParams_childWidthSet() {
        int childWidth = 5;

        ElementList list = ElementList.newBuilder()
                                   .addContents(Content.newBuilder().setElement(IMAGE_ELEMENT))
                                   .build();

        mAdapter.createAdapter(asElement(list), mFrameContext);
        StyleProvider childStyleProvider = mock(StyleProvider.class);
        ImageElementAdapter mockChildAdapter = mock(ImageElementAdapter.class);
        when(mockChildAdapter.getComputedWidthPx()).thenReturn(childWidth);
        when(mockChildAdapter.getElementStyle()).thenReturn(childStyleProvider);
        when(mockChildAdapter.getHorizontalGravity(anyInt())).thenReturn(Gravity.CENTER_HORIZONTAL);
        when(mockChildAdapter.getElementStyle().hasGravityHorizontal()).thenReturn(true);
        mAdapter.mChildAdapters.set(0, mockChildAdapter);

        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        mAdapter.setLayoutParams(params);

        ArgumentCaptor<LinearLayout.LayoutParams> childLayoutParamsCaptor =
                ArgumentCaptor.forClass(LinearLayout.LayoutParams.class);
        verify(mockChildAdapter).setLayoutParams(childLayoutParamsCaptor.capture());

        LinearLayout.LayoutParams childLayoutParams = childLayoutParamsCaptor.getValue();
        assertThat(childLayoutParams.width).isEqualTo(childWidth);
        assertThat(childLayoutParams.gravity).isEqualTo(Gravity.CENTER_HORIZONTAL);

        verify(childStyleProvider).applyMargins(mContext, childLayoutParams);
    }

    @Test
    public void testSetLayoutParams_widthSetOnList() {
        ElementList list = ElementList.newBuilder()
                                   .addContents(Content.newBuilder().setElement(IMAGE_ELEMENT))
                                   .build();

        mAdapter.createAdapter(asElement(list), mFrameContext);

        LinearLayout.LayoutParams params =
                new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.weight = 1;
        mAdapter.setLayoutParams(params);

        assertThat(mAdapter.getBaseView().getChildCount()).isEqualTo(1);
        assertThat(mAdapter.getBaseView().getChildAt(0).getLayoutParams())
                .isInstanceOf(LinearLayout.LayoutParams.class);
        assertThat(((LayoutParams) mAdapter.getBaseView().getChildAt(0).getLayoutParams()).width)
                .isEqualTo(ViewGroup.LayoutParams.MATCH_PARENT);
        assertThat(((LayoutParams) mAdapter.getBaseView().getChildAt(0).getLayoutParams()).height)
                .isEqualTo(ViewGroup.LayoutParams.WRAP_CONTENT);
    }

    private static Element asElement(ElementList elementList) {
        return Element.newBuilder().setElementList(elementList).build();
    }

    private static Element asElement(ElementList elementList, StyleIdsStack styles) {
        return Element.newBuilder().setStyleReferences(styles).setElementList(elementList).build();
    }

    private static Element asElementWithDefaultStyle(ElementList elementList) {
        return Element.newBuilder()
                .setStyleReferences(LIST_STYLES)
                .setElementList(elementList)
                .build();
    }
}
