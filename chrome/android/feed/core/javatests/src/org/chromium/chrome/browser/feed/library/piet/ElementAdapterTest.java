// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.chrome.browser.feed.library.piet.StyleProvider.DIMENSION_NOT_SET;

import android.app.Activity;
import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;

import org.chromium.chrome.browser.feed.library.common.functional.Suppliers;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler.ActionType;
import org.chromium.chrome.browser.feed.library.piet.host.LogDataCallback;
import org.chromium.chrome.browser.feed.library.piet.ui.RoundedCornerMaskCache;
import org.chromium.chrome.browser.feed.library.piet.ui.RoundedCornerWrapperView;
import org.chromium.chrome.browser.feed.library.testing.shadows.ExtendedShadowView;
import org.chromium.components.feed.core.proto.ui.piet.AccessibilityProto.Accessibility;
import org.chromium.components.feed.core.proto.ui.piet.AccessibilityProto.AccessibilityRole;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.Action;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.Actions;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.VisibilityAction;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ActionsBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.LogDataBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.ParameterizedTextBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.BindingRefsProto.VisibilityBindingRef;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingValue;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Visibility;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.VisibilityState;
import org.chromium.components.feed.core.proto.ui.piet.LogDataProto.LogData;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Frame;
import org.chromium.components.feed.core.proto.ui.piet.RoundedCornersProto.RoundedCorners;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.Borders;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.StyleIdsStack;
import org.chromium.components.feed.core.proto.ui.piet.TextProto.ParameterizedText;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link ElementAdapter}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ElementAdapterTest {
    private static final int WIDTH = 123;
    private static final int HEIGHT = 456;
    private static final boolean LEGACY_CORNERS_FLAG = false;
    private static final boolean OUTLINE_CORNERS_FLAG = false;

    private final LogData mLogDataTest = LogData.newBuilder().build();

    @Mock
    private FrameContext mFrameContext;
    @Mock
    private HostProviders mHostProviders;
    @Mock
    private ActionHandler mActionHandler;
    @Mock
    private StyleProvider mStyleProvider;

    private Context mContext;
    private AdapterParameters mParameters;
    private View mView;
    private RoundedCornerMaskCache mMaskCache;

    private TestElementAdapter mAdapter;
    private boolean mCallbackBound;
    private boolean mCallbackUnbound;

    @Before
    public void setUp() {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        LogDataCallback logDataCallback = new LogDataCallback() {
            @Override
            public void onBind(LogData logData, View view) {
                assertThat(mLogDataTest).isEqualTo(logData);
                mCallbackBound = true;
            }

            @Override
            public void onUnbind(LogData logData, View view) {
                assertThat(mLogDataTest).isEqualTo(logData);
                mCallbackUnbound = true;
            }
        };
        mParameters = new AdapterParameters(mContext, Suppliers.of((ViewGroup) null),
                mHostProviders, new FakeClock(), LEGACY_CORNERS_FLAG, OUTLINE_CORNERS_FLAG);
        mMaskCache = mParameters.mRoundedCornerMaskCache;
        when(mHostProviders.getLogDataCallback()).thenReturn(logDataCallback);
        when(mFrameContext.makeStyleFor(any(StyleIdsStack.class))).thenReturn(mStyleProvider);
        when(mFrameContext.getActionHandler()).thenReturn(mActionHandler);
        when(mStyleProvider.hasRoundedCorners()).thenReturn(false);
        when(mStyleProvider.getRoundedCorners()).thenReturn(RoundedCorners.getDefaultInstance());
        when(mStyleProvider.createWrapperView(
                     mContext, mMaskCache, LEGACY_CORNERS_FLAG, OUTLINE_CORNERS_FLAG))
                .thenReturn(new FrameLayout(mContext));

        mView = new View(mContext);
        mAdapter = new TestElementAdapter(mContext, mParameters, mView);
    }

    @Test
    public void testGetters() {
        // Pre-creation
        assertThat(mAdapter.getHorizontalGravity(Gravity.CLIP_HORIZONTAL))
                .isEqualTo(Gravity.CLIP_HORIZONTAL);
        assertThat(mAdapter.getVerticalGravity(Gravity.CLIP_VERTICAL))
                .isEqualTo(Gravity.CLIP_VERTICAL);
        assertThat(mAdapter.getGravity(Gravity.AXIS_Y_SHIFT)).isEqualTo(Gravity.AXIS_Y_SHIFT);

        Element defaultElement = Element.newBuilder().build();
        Frame frame = Frame.newBuilder().setTag("FRAME").build();
        when(mFrameContext.getFrame()).thenReturn(frame);
        when(mStyleProvider.getGravityHorizontal(Gravity.CLIP_HORIZONTAL))
                .thenReturn(Gravity.CENTER_HORIZONTAL);
        when(mStyleProvider.hasGravityHorizontal()).thenReturn(true);
        when(mStyleProvider.hasGravityVertical()).thenReturn(true);
        when(mStyleProvider.getGravityHorizontal(anyInt())).thenReturn(Gravity.CENTER_HORIZONTAL);
        when(mStyleProvider.getGravityVertical(anyInt())).thenReturn(Gravity.CENTER_VERTICAL);
        // This should typically be CENTER_HORIZONTAL | CENTER_VERTICAL but I want to make sure that
        // ElementAdapter is calling this method instead of OR'ing the H/V gravities itself.
        when(mStyleProvider.getGravity(anyInt())).thenReturn(Gravity.FILL);

        mAdapter.createAdapter(defaultElement, defaultElement, mFrameContext);

        assertThat(mAdapter.getContext()).isSameInstanceAs(mContext);
        assertThat(mAdapter.getModel()).isSameInstanceAs(defaultElement);
        assertThat(mAdapter.getRawModel()).isSameInstanceAs(defaultElement);
        assertThat(mAdapter.getHorizontalGravity(Gravity.CLIP_HORIZONTAL))
                .isEqualTo(Gravity.CENTER_HORIZONTAL);
        assertThat(mAdapter.getParameters()).isSameInstanceAs(mParameters);
        assertThat(mAdapter.getTemplatedStringEvaluator())
                .isSameInstanceAs(mParameters.mTemplatedStringEvaluator);
        assertThat(mAdapter.getElementStyle()).isSameInstanceAs(mStyleProvider);
        assertThat(mAdapter.getElementStyleIdsStack())
                .isEqualTo(StyleIdsStack.getDefaultInstance());

        assertThat(mAdapter.getHorizontalGravity(Gravity.CLIP_VERTICAL))
                .isEqualTo(Gravity.CENTER_HORIZONTAL);
        assertThat(mAdapter.getVerticalGravity(Gravity.CLIP_HORIZONTAL))
                .isEqualTo(Gravity.CENTER_VERTICAL);
        assertThat(mAdapter.getGravity(Gravity.CLIP_VERTICAL)).isEqualTo(Gravity.FILL);
    }

    @Test
    public void setLayoutParams() {
        mAdapter.setLayoutParams(new LayoutParams(WIDTH, HEIGHT));

        assertThat(mView.getLayoutParams().width).isEqualTo(WIDTH);
        assertThat(mView.getLayoutParams().height).isEqualTo(HEIGHT);
    }

    @Test
    public void testGetComputedDimensions_defaults() {
        assertThat(mAdapter.getComputedWidthPx()).isEqualTo(DIMENSION_NOT_SET);
        assertThat(mAdapter.getComputedHeightPx()).isEqualTo(DIMENSION_NOT_SET);
    }

    @Test
    public void getComputedDimensions_explicit() {
        mAdapter.setDims(WIDTH, HEIGHT);

        assertThat(mAdapter.getComputedWidthPx()).isEqualTo(WIDTH);
        assertThat(mAdapter.getComputedHeightPx()).isEqualTo(HEIGHT);
    }

    @Test
    public void createAdapter_callsOnCreateAdapter() {
        Element defaultElement = Element.getDefaultInstance();

        mAdapter.createAdapter(defaultElement, defaultElement, mFrameContext);

        assertThat(mAdapter.mTestAdapterCreated).isTrue();
    }

    @Test
    public void createAdapter_extractsModelFromElement() {
        Element element = Element.getDefaultInstance();

        mAdapter.createAdapter(element, mFrameContext);

        assertThat(mAdapter.getModel()).isEqualTo(TestElementAdapter.DEFAULT_MODEL);
    }

    @Test
    public void testCreateAdapter_addsBorders() {
        when(mStyleProvider.hasBorders()).thenReturn(true);

        mAdapter.createAdapter(
                Element.getDefaultInstance(), Element.getDefaultInstance(), mFrameContext);

        verify(mStyleProvider)
                .addBordersWithoutRoundedCorners((FrameLayout) mAdapter.getView(), mContext);
    }

    @Test
    public void createAdapter_appliesVisibility() {
        Element defaultElement =
                Element.newBuilder()
                        .setVisibilityState(VisibilityState.newBuilder().setDefaultVisibility(
                                Visibility.INVISIBLE))
                        .build();

        mAdapter.createAdapter(defaultElement, mFrameContext);

        assertThat(mAdapter.getView().getVisibility()).isEqualTo(View.INVISIBLE);
    }

    @Test
    public void createAdapter_doesNothingWithVisibilityGone() {
        Element defaultElement =
                Element.newBuilder()
                        .setVisibilityState(
                                VisibilityState.newBuilder().setDefaultVisibility(Visibility.GONE))
                        .build();

        mAdapter.createAdapter(defaultElement, mFrameContext);

        assertThat(mAdapter.mTestAdapterCreated).isFalse();
        assertThat(mAdapter.getView().getVisibility()).isEqualTo(View.GONE);
    }

    @Test
    public void createAdapter_ignoresBoundVisibility() {
        String visibilityBindingId = "invisible";
        VisibilityBindingRef visibilityBinding =
                VisibilityBindingRef.newBuilder().setBindingId(visibilityBindingId).build();
        Element defaultElement =
                Element.newBuilder()
                        .setVisibilityState(
                                VisibilityState.newBuilder()
                                        .setDefaultVisibility(Visibility.VISIBLE)
                                        .setOverridingBoundVisibility(visibilityBinding))
                        .build();
        when(mFrameContext.getVisibilityFromBinding(visibilityBinding)).thenReturn(Visibility.GONE);

        mAdapter.createAdapter(defaultElement, mFrameContext);

        assertThat(mAdapter.mTestAdapterCreated).isTrue();
        assertThat(mAdapter.getView().getVisibility()).isEqualTo(View.VISIBLE);
    }

    @Test
    public void createAdapter_setsDimensions() {
        int width = 10;
        int height = 20;

        when(mStyleProvider.hasWidth()).thenReturn(true);
        when(mStyleProvider.hasHeight()).thenReturn(true);
        when(mStyleProvider.getWidthSpecPx(mContext)).thenReturn(width);
        when(mStyleProvider.getHeightSpecPx(mContext)).thenReturn(height);

        mAdapter.createAdapter(Element.getDefaultInstance(), mFrameContext);

        assertThat(mAdapter.getComputedWidthPx()).isEqualTo(width);
        assertThat(mAdapter.getComputedHeightPx()).isEqualTo(height);
    }

    @Test
    public void createAdapter_resetsUnsetDimensions() {
        // Set some dimensions first
        int width = 10;
        int height = 20;

        when(mStyleProvider.hasWidth()).thenReturn(true);
        when(mStyleProvider.hasHeight()).thenReturn(true);
        when(mStyleProvider.getWidthSpecPx(mContext)).thenReturn(width);
        when(mStyleProvider.getHeightSpecPx(mContext)).thenReturn(height);

        mAdapter.createAdapter(Element.getDefaultInstance(), mFrameContext);
        mAdapter.releaseAdapter();

        // Recreate with new style that unsets the dimensions
        when(mStyleProvider.hasWidth()).thenReturn(false);
        when(mStyleProvider.hasHeight()).thenReturn(false);
        when(mStyleProvider.getWidthSpecPx(mContext)).thenReturn(DIMENSION_NOT_SET);
        when(mStyleProvider.getHeightSpecPx(mContext)).thenReturn(DIMENSION_NOT_SET);

        mAdapter.createAdapter(Element.getDefaultInstance(), mFrameContext);

        assertThat(mAdapter.getComputedWidthPx()).isEqualTo(DIMENSION_NOT_SET);
        assertThat(mAdapter.getComputedHeightPx()).isEqualTo(DIMENSION_NOT_SET);
    }

    @Test
    public void createAdapter_dimensionsDontOverrideSubclass() {
        int width = 10;
        int height = 20;

        final int subclassWidth = 30;
        final int subclassHeight = 40;

        mAdapter = new TestElementAdapter(mContext, mParameters, mView) {
            @Override
            protected void onCreateAdapter(
                    Object model, Element baseElement, FrameContext frameContext) {
                mWidthPx = subclassWidth;
                mHeightPx = subclassHeight;
            }
        };

        when(mStyleProvider.hasWidth()).thenReturn(true);
        when(mStyleProvider.hasHeight()).thenReturn(true);
        when(mStyleProvider.getWidthSpecPx(mContext)).thenReturn(width);
        when(mStyleProvider.getHeightSpecPx(mContext)).thenReturn(height);

        mAdapter.createAdapter(Element.getDefaultInstance(), mFrameContext);

        assertThat(mAdapter.getComputedWidthPx()).isEqualTo(subclassWidth);
        assertThat(mAdapter.getComputedHeightPx()).isEqualTo(subclassHeight);
    }

    @Test
    public void bindModel_callsOnBindModel() {
        Element defaultElement = Element.getDefaultInstance();

        mAdapter.bindModel(defaultElement, defaultElement, mFrameContext);

        assertThat(mAdapter.mTestAdapterBound).isTrue();
    }

    @Test
    public void testBindModel_callsOnBindLogDataCallback() {
        Element defaultElement = Element.newBuilder().setLogData(mLogDataTest).build();

        mAdapter.bindModel(defaultElement, defaultElement, mFrameContext);

        assertThat(mCallbackBound).isTrue();
    }

    @Test
    public void testBindModel_callsOnBindLogDataCallback_bindingRef() {
        LogDataBindingRef logDataBinding =
                LogDataBindingRef.newBuilder().setBindingId("LogData!").build();
        Element defaultElement = Element.newBuilder().setLogDataRef(logDataBinding).build();
        when(mFrameContext.getLogDataFromBinding(logDataBinding)).thenReturn(mLogDataTest);
        mAdapter.bindModel(defaultElement, defaultElement, mFrameContext);

        assertThat(mCallbackBound).isTrue();
    }

    @Test
    public void bindModel_extractsModelFromElement() {
        Element element = Element.getDefaultInstance();

        mAdapter.bindModel(element, mFrameContext);

        assertThat(mAdapter.getModel()).isEqualTo(TestElementAdapter.DEFAULT_MODEL);
    }

    @Test
    public void testBindModel_setsActions() {
        Actions actions =
                Actions.newBuilder().setOnClickAction(Action.getDefaultInstance()).build();
        Element elementWithActions = Element.newBuilder().setActions(actions).build();

        mAdapter.bindModel(elementWithActions, elementWithActions, mFrameContext);
        assertThat(mAdapter.getView().hasOnClickListeners()).isTrue();
        assertThat(mAdapter.mActions).isSameInstanceAs(actions);
    }

    @Test
    public void bindModel_setsActionsOnBaseView() {
        Actions actions =
                Actions.newBuilder().setOnClickAction(Action.getDefaultInstance()).build();
        Element elementWithActions = Element.newBuilder().setActions(actions).build();
        when(mStyleProvider.hasBorders()).thenReturn(true);

        mAdapter.createAdapter(elementWithActions, elementWithActions, mFrameContext);
        mAdapter.bindModel(elementWithActions, elementWithActions, mFrameContext);
        assertThat(mAdapter.getView().hasOnClickListeners()).isFalse();
        assertThat(mAdapter.getBaseView().hasOnClickListeners()).isTrue();
        assertThat(mAdapter.mActions).isSameInstanceAs(actions);
    }

    @Test
    public void bindModel_setsActionsWithBinding() {
        Actions actions =
                Actions.newBuilder().setOnClickAction(Action.getDefaultInstance()).build();
        ActionsBindingRef actionsBinding =
                ActionsBindingRef.newBuilder().setBindingId("ACTION!").build();
        Element elementWithActions = Element.newBuilder().setActionsBinding(actionsBinding).build();
        when(mFrameContext.getActionsFromBinding(actionsBinding)).thenReturn(actions);

        mAdapter.bindModel(elementWithActions, elementWithActions, mFrameContext);
        assertThat(mAdapter.getView().hasOnClickListeners()).isTrue();
        assertThat(mAdapter.mActions).isSameInstanceAs(actions);
    }

    @Test
    public void bindModel_unsetsActions() {
        Actions actions =
                Actions.newBuilder().setOnClickAction(Action.getDefaultInstance()).build();
        Element elementWithActions = Element.newBuilder().setActions(actions).build();
        Element elementWithoutActions = Element.getDefaultInstance();

        mAdapter.bindModel(elementWithActions, elementWithActions, mFrameContext);
        assertThat(mAdapter.getView().hasOnClickListeners()).isTrue();
        assertThat(mAdapter.mActions).isSameInstanceAs(actions);

        mAdapter.bindModel(elementWithoutActions, elementWithoutActions, mFrameContext);
        assertThat(mAdapter.getView().hasOnClickListeners()).isFalse();
        assertThat(mAdapter.mActions).isSameInstanceAs(Actions.getDefaultInstance());
    }

    @Test
    public void bindModel_unsetsActionsOnBaseView() {
        Actions actions =
                Actions.newBuilder().setOnClickAction(Action.getDefaultInstance()).build();
        Element elementWithActions = Element.newBuilder().setActions(actions).build();
        Element elementWithoutActions = elementWithActions.toBuilder().clearActions().build();

        when(mStyleProvider.hasBorders()).thenReturn(true);
        mAdapter.createAdapter(elementWithActions, elementWithActions, mFrameContext);
        mAdapter.bindModel(elementWithActions, elementWithActions, mFrameContext);
        assertThat(mAdapter.getView().hasOnClickListeners()).isFalse();
        assertThat(mAdapter.getBaseView().hasOnClickListeners()).isTrue();
        assertThat(mAdapter.mActions).isSameInstanceAs(actions);

        mAdapter.unbindModel();

        when(mStyleProvider.hasBorders()).thenReturn(false);
        mAdapter.bindModel(elementWithoutActions, elementWithoutActions, mFrameContext);
        assertThat(mAdapter.getView().hasOnClickListeners()).isFalse();
        assertThat(mAdapter.getBaseView().hasOnClickListeners()).isFalse();
        assertThat(mAdapter.mActions).isSameInstanceAs(Actions.getDefaultInstance());
    }

    @Test
    public void testBindModel_setsVisibility() {
        Element visibleElement =
                Element.newBuilder()
                        .setVisibilityState(VisibilityState.newBuilder().setDefaultVisibility(
                                Visibility.VISIBLE))
                        .build();
        mAdapter.createAdapter(visibleElement, mFrameContext);
        Element invisibleElement =
                Element.newBuilder()
                        .setVisibilityState(VisibilityState.newBuilder().setDefaultVisibility(
                                Visibility.INVISIBLE))
                        .build();

        mAdapter.bindModel(invisibleElement, mFrameContext);

        assertThat(mAdapter.mTestAdapterBound).isTrue();
        assertThat(mAdapter.getView().getVisibility()).isEqualTo(View.INVISIBLE);
    }

    @Test
    public void bindModel_doesNothingWhenVisibilityIsGone() {
        Element visibleElement =
                Element.newBuilder()
                        .setVisibilityState(VisibilityState.newBuilder().setDefaultVisibility(
                                Visibility.VISIBLE))
                        .build();
        mAdapter.createAdapter(visibleElement, mFrameContext);
        Element goneElement =
                Element.newBuilder()
                        .setVisibilityState(
                                VisibilityState.newBuilder().setDefaultVisibility(Visibility.GONE))
                        .build();

        mAdapter.bindModel(goneElement, mFrameContext);

        assertThat(mAdapter.mTestAdapterBound).isFalse();
        assertThat(mAdapter.getView().getVisibility()).isEqualTo(View.GONE);
    }

    @Test
    public void bindModel_recreatesWhenVisibilityWasGone() {
        Element goneElement =
                Element.newBuilder()
                        .setVisibilityState(
                                VisibilityState.newBuilder().setDefaultVisibility(Visibility.GONE))
                        .build();
        mAdapter.createAdapter(goneElement, mFrameContext);
        Element invisibleElement =
                Element.newBuilder()
                        .setVisibilityState(VisibilityState.newBuilder().setDefaultVisibility(
                                Visibility.INVISIBLE))
                        .build();

        mAdapter.bindModel(invisibleElement, mFrameContext);

        assertThat(mAdapter.mTestAdapterCreated).isTrue();
        assertThat(mAdapter.mTestAdapterBound).isTrue();
        assertThat(mAdapter.getView().getVisibility()).isEqualTo(View.INVISIBLE);
    }

    @Test
    public void bindModel_recreatesWhenVisibilityWasGone_bound() {
        VisibilityBindingRef visibilityBinding =
                VisibilityBindingRef.newBuilder().setBindingId("camo").build();
        Element elementWithVisibilityBinding =
                Element.newBuilder()
                        .setVisibilityState(
                                VisibilityState.newBuilder()
                                        .setDefaultVisibility(Visibility.GONE)
                                        .setOverridingBoundVisibility(visibilityBinding))
                        .build();

        // When adapter is created, the Element is GONE
        when(mFrameContext.getVisibilityFromBinding(visibilityBinding)).thenReturn(Visibility.GONE);

        mAdapter.createAdapter(elementWithVisibilityBinding, mFrameContext);

        assertThat(mAdapter.mTestAdapterCreated).isFalse();
        assertThat(mAdapter.getView().getVisibility()).isEqualTo(View.GONE);

        // Now on binding, the Element is VISIBLE
        when(mFrameContext.getVisibilityFromBinding(visibilityBinding))
                .thenReturn(Visibility.VISIBLE);

        mAdapter.bindModel(elementWithVisibilityBinding, mFrameContext);

        assertThat(mAdapter.mTestAdapterCreated).isTrue();
        assertThat(mAdapter.mTestAdapterBound).isTrue();
        assertThat(mAdapter.getView().getVisibility()).isEqualTo(View.VISIBLE);
    }

    @Test
    public void bindModel_doesntRecreateWhenWasCreatedButVisibilityGone() {
        // Create a real adapter for a visible element.
        Element visibileElement =
                Element.newBuilder()
                        .setVisibilityState(VisibilityState.newBuilder().setDefaultVisibility(
                                Visibility.VISIBLE))
                        .build();
        mAdapter.createAdapter(visibileElement, mFrameContext);

        // Bind a gone element, and unbind it so the visibility is GONE.
        Element goneElement =
                Element.newBuilder()
                        .setVisibilityState(
                                VisibilityState.newBuilder().setDefaultVisibility(Visibility.GONE))
                        .build();
        mAdapter.bindModel(goneElement, mFrameContext);
        mAdapter.unbindModel();

        assertThat(mAdapter.mTestAdapterCreated).isTrue();
        mAdapter.mTestAdapterCreated =
                false; // set this so we can test that it doesn't get recreated

        // Bind a real element, and ensure that the adapter is not recreated.
        Element invisibleElement =
                Element.newBuilder()
                        .setVisibilityState(VisibilityState.newBuilder().setDefaultVisibility(
                                Visibility.INVISIBLE))
                        .build();

        mAdapter.bindModel(invisibleElement, mFrameContext);

        assertThat(mAdapter.mTestAdapterCreated).isFalse();
        assertThat(mAdapter.mTestAdapterBound).isTrue();
        assertThat(mAdapter.getView().getVisibility()).isEqualTo(View.INVISIBLE);
    }

    @Test
    public void bindModel_accessibility() {
        Element accessibilityElement =
                Element.newBuilder()
                        .setAccessibility(
                                Accessibility.newBuilder()
                                        .addRoles(AccessibilityRole.HEADER)
                                        .setDescription(ParameterizedText.newBuilder().setText(
                                                "Accessible!")))
                        .build();

        mAdapter.bindModel(accessibilityElement, mFrameContext);
        assertThat(mAdapter.getView().getContentDescription().toString()).isEqualTo("Accessible!");
        final AccessibilityNodeInfo nodeInfo = AccessibilityNodeInfo.obtain();
        mAdapter.getView().onInitializeAccessibilityNodeInfo(nodeInfo);
        // TODO(crbug.com/1024945): This test fails with NPE.
        // assertThat(nodeInfo.getCollectionItemInfo().isHeading()).isTrue();
    }

    @Test
    public void bindModel_accessibilityBinding() {
        ParameterizedTextBindingRef textBinding =
                ParameterizedTextBindingRef.newBuilder().setBindingId("binding").build();
        Element accessibilityElement =
                Element.newBuilder()
                        .setAccessibility(
                                Accessibility.newBuilder().setDescriptionBinding(textBinding))
                        .build();
        when(mFrameContext.getParameterizedTextBindingValue(textBinding))
                .thenReturn(BindingValue.newBuilder()
                                    .setParameterizedText(
                                            ParameterizedText.newBuilder().setText("Accessible!"))
                                    .build());

        mAdapter.bindModel(accessibilityElement, mFrameContext);

        assertThat(mAdapter.getView().getContentDescription().toString()).isEqualTo("Accessible!");
    }

    @Test
    public void bindModel_accessibilityReset() {
        mAdapter.getView().setContentDescription("OLD CONTENT DESCRIPTION");

        mAdapter.bindModel(Element.getDefaultInstance(), mFrameContext);

        assertThat(mAdapter.getView().getContentDescription()).isNull();
    }

    @Test
    public void unbindModel_callsOnUnbindModel() {
        Element defaultElement = Element.getDefaultInstance();

        mAdapter.createAdapter(defaultElement, defaultElement, mFrameContext);
        mAdapter.bindModel(defaultElement, defaultElement, mFrameContext);
        assertThat(mAdapter.mTestAdapterBound).isTrue();

        mAdapter.unbindModel();
        assertThat(mAdapter.mTestAdapterBound).isFalse();
    }

    @Test
    public void testUnbindModel_callsOnBindLogDataCallback() {
        Element defaultElement = Element.newBuilder().setLogData(mLogDataTest).build();

        mAdapter.bindModel(defaultElement, defaultElement, mFrameContext);
        mAdapter.unbindModel();
        assertThat(mCallbackUnbound).isTrue();
    }

    @Test
    public void testUnbindModel_callsOnBindLogDataCallback_bindingRef() {
        LogDataBindingRef logDataBinding =
                LogDataBindingRef.newBuilder().setBindingId("LogData!").build();
        Element defaultElement = Element.newBuilder().setLogDataRef(logDataBinding).build();
        when(mFrameContext.getLogDataFromBinding(logDataBinding)).thenReturn(mLogDataTest);

        mAdapter.bindModel(defaultElement, defaultElement, mFrameContext);
        mAdapter.unbindModel();
        assertThat(mCallbackUnbound).isTrue();
    }

    @Test
    public void unbindModel_unsetsModel() {
        Element element = Element.getDefaultInstance();

        mAdapter.createAdapter(element, element, mFrameContext);
        mAdapter.bindModel(element, element, mFrameContext);
        assertThat(mAdapter.getModel()).isEqualTo(element);

        mAdapter.unbindModel();
        assertThat(mAdapter.getRawModel()).isNull();
    }

    // If onBindModel was never called, onUnbindModel should not be called.
    @Test
    public void unbindModel_visibilityWasGone() {
        Element goneElement =
                Element.newBuilder()
                        .setVisibilityState(
                                VisibilityState.newBuilder().setDefaultVisibility(Visibility.GONE))
                        .build();
        mAdapter.createAdapter(goneElement, mFrameContext);
        mAdapter.bindModel(goneElement, mFrameContext);

        assertThat(mAdapter.mTestAdapterCreated).isFalse();
        assertThat(mAdapter.mTestAdapterBound).isFalse();

        // Set this to something else so we can check if onUnbindModel gets called - it should not.
        mAdapter.mTestAdapterBound = true;

        mAdapter.unbindModel();

        // onUnbindModel was not called so this is still true.
        assertThat(mAdapter.mTestAdapterBound).isTrue();
        assertThat(mAdapter.mTestAdapterCreated).isFalse();
    }

    @Test
    public void unbindModel_unsetsActions() {
        Element elementWithWrapperAndActions =
                Element.newBuilder()
                        .setActions(
                                Actions.newBuilder().setOnClickAction(Action.getDefaultInstance()))
                        .build();

        when(mStyleProvider.hasBorders()).thenReturn(true);

        mAdapter.createAdapter(
                elementWithWrapperAndActions, elementWithWrapperAndActions, mFrameContext);
        mAdapter.bindModel(
                elementWithWrapperAndActions, elementWithWrapperAndActions, mFrameContext);
        View adapterView = mAdapter.getBaseView();
        View wrapperView = mAdapter.getView();

        mAdapter.unbindModel();

        assertThat(adapterView.hasOnClickListeners()).isFalse();
        assertThat(wrapperView.hasOnClickListeners()).isFalse();
    }

    @Test
    public void testUnbindModel_affectsVisibilityCalculations() {
        VisibilityBindingRef visibilityBinding =
                VisibilityBindingRef.newBuilder().setBindingId("vis").build();
        Element element = Element.newBuilder()
                                  .setVisibilityState(
                                          VisibilityState.newBuilder()
                                                  .setDefaultVisibility(Visibility.INVISIBLE)
                                                  .setOverridingBoundVisibility(visibilityBinding))
                                  .build();
        when(mFrameContext.getVisibilityFromBinding(visibilityBinding))
                .thenReturn(Visibility.VISIBLE);

        mAdapter.createAdapter(element, element, mFrameContext);
        assertThat(mAdapter.getVisibilityForElement(element, mFrameContext))
                .isEqualTo(Visibility.INVISIBLE);

        mAdapter.bindModel(element, element, mFrameContext);
        assertThat(mAdapter.getVisibilityForElement(element, mFrameContext))
                .isEqualTo(Visibility.VISIBLE);

        mAdapter.unbindModel();
        assertThat(mAdapter.getVisibilityForElement(element, mFrameContext))
                .isEqualTo(Visibility.INVISIBLE);
    }

    @Test
    public void releaseAdapter_callsOnReleaseAdapter() {
        Element defaultElement = Element.getDefaultInstance();

        mAdapter.createAdapter(defaultElement, defaultElement, mFrameContext);
        assertThat(mAdapter.mTestAdapterCreated).isTrue();

        mAdapter.releaseAdapter();
        assertThat(mAdapter.mTestAdapterCreated).isFalse();
    }

    @Test
    public void testReleaseAdapter_resetsVisibility() {
        Element defaultElement = Element.getDefaultInstance();

        mAdapter.createAdapter(defaultElement, defaultElement, mFrameContext);
        mAdapter.setVisibilityOnView(Visibility.INVISIBLE);
        assertThat(mAdapter.getBaseView().getVisibility()).isEqualTo(View.INVISIBLE);

        mAdapter.releaseAdapter();
        assertThat(mAdapter.getBaseView().getVisibility()).isEqualTo(View.VISIBLE);
    }

    @Test
    public void releaseAdapter_resetsDimensions() {
        Element defaultElement = Element.getDefaultInstance();

        mAdapter.createAdapter(defaultElement, defaultElement, mFrameContext);
        mAdapter.setDims(123, 456);

        mAdapter.releaseAdapter();

        assertThat(mAdapter.getComputedWidthPx()).isEqualTo(DIMENSION_NOT_SET);
        assertThat(mAdapter.getComputedHeightPx()).isEqualTo(DIMENSION_NOT_SET);
    }

    @Test
    public void setVisibility() {
        mAdapter.createAdapter(
                Element.getDefaultInstance(), Element.getDefaultInstance(), mFrameContext);
        mAdapter.getBaseView().setVisibility(View.GONE);
        mAdapter.setVisibilityOnView(Visibility.VISIBLE);
        assertThat(mAdapter.getBaseView().getVisibility()).isEqualTo(View.VISIBLE);
        mAdapter.setVisibilityOnView(Visibility.INVISIBLE);
        assertThat(mAdapter.getBaseView().getVisibility()).isEqualTo(View.INVISIBLE);
        mAdapter.setVisibilityOnView(Visibility.GONE);
        assertThat(mAdapter.getBaseView().getVisibility()).isEqualTo(View.GONE);
        mAdapter.setVisibilityOnView(Visibility.VISIBILITY_UNSPECIFIED);
        assertThat(mAdapter.getBaseView().getVisibility()).isEqualTo(View.VISIBLE);
    }

    @Test
    public void getVisibilityForElement() {
        VisibilityBindingRef bindingRef =
                VisibilityBindingRef.newBuilder().setBindingId("binding").build();
        Element elementWithDefaultVisibility =
                Element.newBuilder()
                        .setVisibilityState(VisibilityState.newBuilder().setDefaultVisibility(
                                Visibility.INVISIBLE))
                        .build();
        Element elementWithVisibilityOverride =
                Element.newBuilder()
                        .setVisibilityState(VisibilityState.newBuilder()
                                                    .setDefaultVisibility(Visibility.INVISIBLE)
                                                    .setOverridingBoundVisibility(bindingRef))
                        .build();

        // Default visibility; no override
        assertThat(mAdapter.getVisibilityForElement(elementWithDefaultVisibility, mFrameContext))
                .isEqualTo(Visibility.INVISIBLE);

        // Don't look up override when not binding
        when(mFrameContext.getVisibilityFromBinding(bindingRef)).thenReturn(Visibility.GONE);
        assertThat(mAdapter.getVisibilityForElement(elementWithVisibilityOverride, mFrameContext))
                .isEqualTo(Visibility.INVISIBLE);

        mAdapter.bindModel(Element.getDefaultInstance(), mFrameContext);

        // Look up override successfully
        when(mFrameContext.getVisibilityFromBinding(bindingRef)).thenReturn(Visibility.GONE);
        assertThat(mAdapter.getVisibilityForElement(elementWithVisibilityOverride, mFrameContext))
                .isEqualTo(Visibility.GONE);

        // Override not found; use default
        when(mFrameContext.getVisibilityFromBinding(bindingRef)).thenReturn(null);
        assertThat(mAdapter.getVisibilityForElement(elementWithVisibilityOverride, mFrameContext))
                .isEqualTo(Visibility.INVISIBLE);
    }

    @Config(shadows = {ExtendedShadowView.class})
    @Test
    public void getOnFullViewActions() {
        Frame frame = Frame.newBuilder().setTag("FRAME").build();
        when(mFrameContext.getFrame()).thenReturn(frame);
        View viewport = new View(mContext);

        // No actions defined
        mAdapter.createAdapter(Element.getDefaultInstance(), mFrameContext);
        mAdapter.bindModel(Element.getDefaultInstance(), mFrameContext);
        mAdapter.triggerViewActions(viewport, mFrameContext);
        verifyZeroInteractions(mActionHandler);
        mAdapter.releaseAdapter();

        // Actions defined, but not fullview actions
        Element elementWithNoViewActions =
                Element.newBuilder()
                        .setActions(
                                Actions.newBuilder().setOnClickAction(Action.newBuilder().build()))
                        .build();
        mAdapter.createAdapter(elementWithNoViewActions, mFrameContext);
        mAdapter.bindModel(elementWithNoViewActions, mFrameContext);
        mAdapter.triggerViewActions(viewport, mFrameContext);
        verifyZeroInteractions(mActionHandler);
        mAdapter.unbindModel();
        mAdapter.releaseAdapter();

        // Actions defined, but not fully visible
        ExtendedShadowView viewShadow = Shadow.extract(mAdapter.getView());
        viewShadow.setAttachedToWindow(false);
        Actions fullViewActions = Actions.newBuilder()
                                          .addOnViewActions(VisibilityAction.newBuilder().setAction(
                                                  Action.newBuilder().build()))
                                          .build();
        Element elementWithActions = Element.newBuilder().setActions(fullViewActions).build();
        mAdapter.createAdapter(elementWithActions, mFrameContext);
        mAdapter.bindModel(elementWithActions, mFrameContext);
        mAdapter.triggerViewActions(viewport, mFrameContext);
        verifyZeroInteractions(mActionHandler);
        mAdapter.unbindModel();
        mAdapter.releaseAdapter();

        // Actions defined, and fully visible
        ExtendedShadowView viewportShadow = Shadow.extract(viewport);
        viewportShadow.setLocationInWindow(0, 0);
        viewportShadow.setHeight(100);
        viewportShadow.setWidth(100);
        viewShadow.setLocationInWindow(10, 10);
        viewShadow.setHeight(50);
        viewShadow.setWidth(50);
        viewShadow.setAttachedToWindow(true);

        mAdapter.createAdapter(elementWithActions, mFrameContext);
        mAdapter.bindModel(elementWithActions, mFrameContext);
        mAdapter.triggerViewActions(viewport, mFrameContext);
        verify(mActionHandler)
                .handleAction(same(fullViewActions.getOnViewActions(0).getAction()),
                        eq(ActionType.VIEW), eq(frame), eq(mView),
                        eq(LogData.getDefaultInstance()));
    }

    @Config(shadows = {ExtendedShadowView.class})
    @Test
    public void getOnPartialViewActions() {
        Frame frame = Frame.newBuilder().setTag("FRAME").build();
        when(mFrameContext.getFrame()).thenReturn(frame);
        View viewport = new View(mContext);

        // No actions defined
        mAdapter.createAdapter(Element.getDefaultInstance(), mFrameContext);
        mAdapter.bindModel(Element.getDefaultInstance(), mFrameContext);
        mAdapter.triggerViewActions(viewport, mFrameContext);
        verifyZeroInteractions(mActionHandler);
        mAdapter.releaseAdapter();

        // Actions defined, but not partial view actions
        Element elementWithNoViewActions =
                Element.newBuilder()
                        .setActions(
                                Actions.newBuilder().setOnClickAction(Action.newBuilder().build()))
                        .build();
        mAdapter.createAdapter(elementWithNoViewActions, mFrameContext);
        mAdapter.bindModel(elementWithNoViewActions, mFrameContext);
        mAdapter.triggerViewActions(viewport, mFrameContext);
        verifyZeroInteractions(mActionHandler);
        mAdapter.unbindModel();
        mAdapter.releaseAdapter();

        // Actions defined, but not attached to window
        ExtendedShadowView viewShadow = Shadow.extract(mAdapter.getView());
        viewShadow.setAttachedToWindow(false);
        Actions partialViewActions =
                Actions.newBuilder()
                        .addOnViewActions(VisibilityAction.newBuilder()
                                                  .setAction(Action.newBuilder().build())
                                                  .setProportionVisible(0.01f))
                        .build();
        Element elementWithActions = Element.newBuilder().setActions(partialViewActions).build();
        mAdapter.createAdapter(elementWithActions, mFrameContext);
        mAdapter.bindModel(elementWithActions, mFrameContext);
        mAdapter.triggerViewActions(viewport, mFrameContext);
        verifyZeroInteractions(mActionHandler);
        mAdapter.unbindModel();
        mAdapter.releaseAdapter();

        // Actions defined, and partially visible
        ExtendedShadowView viewportShadow = Shadow.extract(viewport);
        viewportShadow.setLocationInWindow(0, 0);
        viewportShadow.setHeight(100);
        viewportShadow.setWidth(100);
        viewShadow.setLocationInWindow(10, 10);
        viewShadow.setHeight(200);
        viewShadow.setWidth(200);
        viewShadow.setAttachedToWindow(true);

        mAdapter.createAdapter(elementWithActions, mFrameContext);
        mAdapter.bindModel(elementWithActions, mFrameContext);
        mAdapter.triggerViewActions(viewport, mFrameContext);
        verify(mActionHandler)
                .handleAction(same(partialViewActions.getOnViewActions(0).getAction()),
                        eq(ActionType.VIEW), eq(frame), eq(mView),
                        eq(LogData.getDefaultInstance()));

        // Actions defined, and fully visible
        viewShadow.setHeight(50);
        viewShadow.setWidth(50);

        mAdapter.createAdapter(elementWithActions, mFrameContext);
        mAdapter.bindModel(elementWithActions, mFrameContext);
        mAdapter.triggerViewActions(viewport, mFrameContext);
        verify(mActionHandler, times(2))
                .handleAction(same(partialViewActions.getOnViewActions(0).getAction()),
                        eq(ActionType.VIEW), eq(frame), eq(mView),
                        eq(LogData.getDefaultInstance()));
    }

    @Config(shadows = {ExtendedShadowView.class})
    @Test
    public void triggerHideActions() {
        Frame frame = Frame.newBuilder().setTag("FRAME").build();
        when(mFrameContext.getFrame()).thenReturn(frame);
        View viewport = new View(mContext);

        // No actions defined
        mAdapter.createAdapter(Element.getDefaultInstance(), mFrameContext);
        mAdapter.bindModel(Element.getDefaultInstance(), mFrameContext);
        mAdapter.triggerHideActions(mFrameContext);
        verifyZeroInteractions(mActionHandler);
        mAdapter.releaseAdapter();

        // Actions defined, but not hide actions
        Element elementWithNoViewActions =
                Element.newBuilder()
                        .setActions(
                                Actions.newBuilder().setOnClickAction(Action.getDefaultInstance()))
                        .build();
        mAdapter.createAdapter(elementWithNoViewActions, mFrameContext);
        mAdapter.bindModel(elementWithNoViewActions, mFrameContext);
        mAdapter.triggerHideActions(mFrameContext);
        verifyZeroInteractions(mActionHandler);
        mAdapter.unbindModel();
        mAdapter.releaseAdapter();

        // Trigger hide actions on the fully-visible view to make hide actions not active.
        Action hideAction = Action.newBuilder().build();
        Actions hideActions =
                Actions.newBuilder()
                        .addOnHideActions(VisibilityAction.newBuilder().setAction(hideAction))
                        .build();
        Element elementWithActions = Element.newBuilder().setActions(hideActions).build();
        ExtendedShadowView viewShadow = Shadow.extract(mAdapter.getView());
        ExtendedShadowView viewportShadow = Shadow.extract(viewport);
        viewportShadow.setLocationInWindow(0, 0);
        viewportShadow.setHeight(100);
        viewportShadow.setWidth(100);
        viewShadow.setLocationInWindow(10, 10);
        viewShadow.setHeight(50);
        viewShadow.setWidth(50);
        viewShadow.setAttachedToWindow(true);

        mAdapter.createAdapter(elementWithActions, mFrameContext);
        mAdapter.bindModel(elementWithActions, mFrameContext);
        mAdapter.triggerViewActions(viewport, mFrameContext);
        verifyZeroInteractions(mActionHandler);

        // Trigger hide actions
        mAdapter.triggerHideActions(mFrameContext);

        verify(mActionHandler)
                .handleAction(same(hideAction), eq(ActionType.VIEW), eq(frame), eq(mView),
                        eq(LogData.getDefaultInstance()));

        // Ensure actions do not trigger again
        mAdapter.triggerHideActions(mFrameContext);

        verify(mActionHandler) // Only once, not twice
                .handleAction(same(hideAction), eq(ActionType.VIEW), eq(frame), eq(mView),
                        eq(LogData.getDefaultInstance()));
    }

    @Test
    public void createRoundedCorners() {
        RoundedCorners roundedCorners = RoundedCorners.newBuilder().setRadiusDp(10).build();
        RoundedCornerWrapperView roundedCornerWrapperView = new RoundedCornerWrapperView(mContext,
                roundedCorners, mMaskCache, Suppliers.of(false),
                /*radiusOverride= */ 0, Borders.getDefaultInstance(), LEGACY_CORNERS_FLAG,
                OUTLINE_CORNERS_FLAG);
        when(mStyleProvider.hasRoundedCorners()).thenReturn(true);
        when(mStyleProvider.createWrapperView(
                     mContext, mMaskCache, LEGACY_CORNERS_FLAG, OUTLINE_CORNERS_FLAG))
                .thenReturn(roundedCornerWrapperView);

        mAdapter.createAdapter(Element.getDefaultInstance(), mFrameContext);

        assertThat(mAdapter.getView()).isSameInstanceAs(roundedCornerWrapperView);
    }

    @Test
    public void setVisibilityWithRoundedCorners() {
        RoundedCorners roundedCorners = RoundedCorners.newBuilder().setRadiusDp(10).build();
        RoundedCornerWrapperView roundedCornerWrapperView = new RoundedCornerWrapperView(mContext,
                roundedCorners, mMaskCache, Suppliers.of(false),
                /*radiusOverride= */ 0, Borders.getDefaultInstance(), LEGACY_CORNERS_FLAG,
                OUTLINE_CORNERS_FLAG);
        when(mStyleProvider.hasRoundedCorners()).thenReturn(true);
        when(mStyleProvider.createWrapperView(
                     mContext, mMaskCache, LEGACY_CORNERS_FLAG, OUTLINE_CORNERS_FLAG))
                .thenReturn(roundedCornerWrapperView);

        VisibilityBindingRef visibilityBinding =
                VisibilityBindingRef.newBuilder().setBindingId("visibility").build();
        when(mFrameContext.getVisibilityFromBinding(visibilityBinding))
                .thenReturn(Visibility.VISIBLE);

        Element element = Element.newBuilder()
                                  .setVisibilityState(
                                          VisibilityState.newBuilder()
                                                  .setDefaultVisibility(Visibility.GONE)
                                                  .setOverridingBoundVisibility(visibilityBinding))
                                  .build();

        mAdapter.createAdapter(element, mFrameContext);

        FrameLayout parentView = new FrameLayout(mContext);
        parentView.addView(mAdapter.getView());

        mAdapter.bindModel(element, mFrameContext);

        assertThat(mAdapter.getView()).isSameInstanceAs(roundedCornerWrapperView);
        assertThat(mAdapter.getView().getVisibility()).isEqualTo(View.VISIBLE);

        // Try setting visibility back to GONE and ensure that RCWV is GONE
        when(mFrameContext.getVisibilityFromBinding(visibilityBinding)).thenReturn(Visibility.GONE);
        mAdapter.unbindModel();
        mAdapter.bindModel(element, mFrameContext);
        assertThat(mAdapter.getView()).isSameInstanceAs(roundedCornerWrapperView);
        assertThat(mAdapter.getView().getVisibility()).isEqualTo(View.GONE);
    }

    @Test
    public void getStyleIdsStack() {
        StyleIdsStack style = StyleIdsStack.newBuilder().addStyleIds("style").build();
        Element elementWithStyle = Element.newBuilder().setStyleReferences(style).build();

        mAdapter.createAdapter(elementWithStyle, mFrameContext);

        assertThat(mAdapter.getElementStyleIdsStack()).isSameInstanceAs(style);

        Element elementWithNoStyle = Element.getDefaultInstance();

        mAdapter.createAdapter(elementWithNoStyle, mFrameContext);

        assertThat(mAdapter.getElementStyleIdsStack())
                .isEqualTo(StyleIdsStack.getDefaultInstance());
    }

    // Dummy implementation
    static class TestElementAdapter extends ElementAdapter<View, Object> {
        static final String DEFAULT_MODEL = "MODEL";

        boolean mTestAdapterCreated;
        boolean mTestAdapterBound;

        View mAdapterView;

        TestElementAdapter(Context context, AdapterParameters parameters, View adapterView) {
            super(context, parameters, adapterView);
            this.mAdapterView = adapterView;
        }

        @Override
        protected Object getModelFromElement(Element baseElement) {
            return DEFAULT_MODEL;
        }

        @Override
        protected void onCreateAdapter(
                Object model, Element baseElement, FrameContext frameContext) {
            mTestAdapterCreated = true;
        }

        @Override
        protected void onBindModel(Object model, Element baseElement, FrameContext frameContext) {
            mTestAdapterBound = true;
        }

        @Override
        protected void onUnbindModel() {
            mTestAdapterBound = false;
        }

        @Override
        protected void onReleaseAdapter() {
            mTestAdapterCreated = false;
        }

        public void setDims(int width, int height) {
            mWidthPx = width;
            mHeightPx = height;
        }
    }
}
