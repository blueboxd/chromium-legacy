// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkNotNull;
import static org.chromium.chrome.browser.feed.library.piet.StyleProvider.DIMENSION_NOT_SET;

import android.content.Context;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.piet.DebugLogger.MessageType;
import org.chromium.chrome.browser.feed.library.piet.host.LogDataCallback;
import org.chromium.chrome.browser.feed.library.piet.ui.RoundedCornerWrapperView;
import org.chromium.components.feed.core.proto.ui.piet.AccessibilityProto.Accessibility;
import org.chromium.components.feed.core.proto.ui.piet.AccessibilityProto.AccessibilityRole;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.Actions;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.VisibilityAction;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingValue;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Visibility;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.VisibilityState;
import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;
import org.chromium.components.feed.core.proto.ui.piet.LogDataProto.LogData;
import org.chromium.components.feed.core.proto.ui.piet.StylesProto.StyleIdsStack;

import java.util.HashSet;
import java.util.Set;

/**
 * ElementAdapter provides a base class for the various adapters created by UiElement, and provides
 * handling for overlays and any other general UiElement features. This class enforces the life
 * cycle of the BoundViewAdapter.
 *
 * @param <V> A subclass of an Android View. This is the root level view the adapter is responsible
 *     for creating. Once created, it should have the same lifetime as the adapter.
 * @param <M> The model. This is the model type which is bound to the view by the adapter.
 */
abstract class ElementAdapter<V extends View, M> {
    private static final String TAG = "ElementAdapter";
    private final Context mContext;
    private final AdapterParameters mParameters;
    private final V mView;
    @Nullable
    private M mModel;
    private Element mBaseElement = Element.getDefaultInstance();
    @Nullable
    private RecyclerKey mKey;
    private StyleProvider mElementStyle;
    @Nullable
    private LogData mLogData;

    /**
     * Set to {@code true} when {@link #createAdapter} has completed successfully; unset at the end
     * of
     * {@link #releaseAdapter}.
     */
    private boolean mCreated;

    /**
     * We only check the bound visibility when we're in the process of binding, not in {@link
     * #createAdapter}.
     */
    private boolean mCheckBoundVisibility;

    @Nullable
    private FrameLayout mWrapperView;

    Actions mActions = Actions.getDefaultInstance();
    /** Set of actions that are currently active / triggered so they only get called once. */
    private final Set<VisibilityAction> mActiveVisibilityActions = new HashSet<>();

    /**
     * Desired width of this element. {@link StyleProvider#DIMENSION_NOT_SET} indicates no explicit
     * width; let the parent view decide.
     */
    int mWidthPx = DIMENSION_NOT_SET;
    /**
     * Desired height of this element. {@link StyleProvider#DIMENSION_NOT_SET} indicates no explicit
     * height; let the parent view decide.
     */
    int mHeightPx = DIMENSION_NOT_SET;

    ElementAdapter(Context context, AdapterParameters parameters, V view) {
        this.mContext = context;
        this.mParameters = parameters;
        this.mView = view;
        mElementStyle = parameters.mDefaultStyleProvider;
    }

    ElementAdapter(Context context, AdapterParameters parameters, V view, RecyclerKey key) {
        this(context, parameters, view);
        this.mKey = key;
    }

    /**
     * Returns the View bound to the Adapter, for external use, such as adding to a layout. This is
     * not a valid call until the first Model has been bound to the Adapter.
     */
    public View getView() {
        return mWrapperView != null ? mWrapperView : mView;
    }

    /**
     * Returns the base view of the adapter; the view typed to the subclass's content. This returns
     * a different result than getView when there is a wrapper FrameLayout to support overlays.
     */
    protected V getBaseView() {
        return mView;
    }

    /**
     * Sets up an adapter, but does not bind data. After this, the {@link #getView()} and {@link
     * #getKey()} can be called. This method must not be called if a Model is currently bound to the
     * Adapter; call {@link #releaseAdapter()} first. Do not override; override {@link
     * #onCreateAdapter} instead.
     */
    public void createAdapter(Element baseElement, FrameContext frameContext) {
        M model = getModelFromElement(baseElement);
        createAdapter(model, baseElement, frameContext);
    }

    /**
     * Sets up an adapter, but does not bind data. After this, the {@link #getView()} and {@link
     * #getKey()} can be called. This method must not be called if a Model is currently bound to the
     * Adapter; call {@link #releaseAdapter()} first. Do not override; override {@link
     * #onCreateAdapter} instead.
     */
    public void createAdapter(M model, Element baseElement, FrameContext frameContext) {
        this.mModel = model;
        this.mBaseElement = baseElement;

        // Initialize the wrapper view before checking visibility - we need to have it in place
        // before the parent adds our outermost view to the hierarchy.
        mElementStyle = frameContext.makeStyleFor(getElementStyleIdsStack());
        initializeWrapperViewDependentFeatures();

        Visibility visibility = getVisibilityForElement(baseElement, frameContext);
        setVisibilityOnView(visibility);
        if (visibility == Visibility.GONE) {
            return;
        }

        setDesiredDimensions();

        onCreateAdapter(model, baseElement, frameContext);

        initializeOverflow(baseElement);

        mElementStyle.applyElementStyles(this);
        mCreated = true;
    }

    private void initializeWrapperViewDependentFeatures() {
        if (mWrapperView != null) {
            // We have already initialized the wrapper view.
            return;
        }
        if (!mElementStyle.hasRoundedCorners() && !mElementStyle.hasBorders()) {
            // We do not need a wrapper view.
            return;
        }

        FrameLayout wrapper = mElementStyle.createWrapperView(mContext,
                mParameters.mRoundedCornerMaskCache, mParameters.mAllowLegacyRoundedCornerImpl,
                mParameters.mAllowOutlineRoundedCornerImpl);
        wrapper.addView(getBaseView());

        if (!(wrapper instanceof RoundedCornerWrapperView)) {
            mElementStyle.addBordersWithoutRoundedCorners(wrapper, getContext());
        }

        mWrapperView = wrapper;
    }

    /** Performs child-class-specific adapter creation; to be overridden. */
    void onCreateAdapter(M model, Element baseElement, FrameContext frameContext) {}

    /**
     * Binds data from a Model to the Adapter, without changing child views or styles. Do not
     * override; override {@link #onBindModel} instead. Binds to an Element, allowing the subclass
     * to pick out the relevant model from the oneof.
     */
    public void bindModel(Element baseElement, FrameContext frameContext) {
        bindModel(getModelFromElement(baseElement), baseElement, frameContext);
    }

    /** Bind the model and initialize overlays. Calls child {@link #onBindModel} methods. */
    void bindModel(M model, Element baseElement, FrameContext frameContext) {
        this.mModel = model;
        this.mBaseElement = baseElement;
        mCheckBoundVisibility = true;
        if (getElementStyleIdsStack().hasStyleBinding()) {
            mElementStyle = frameContext.makeStyleFor(getElementStyleIdsStack());
        }

        Visibility visibility = getVisibilityForElement(baseElement, frameContext);
        setVisibilityOnView(visibility);
        if (visibility == Visibility.GONE) {
            return;
        }
        if (!mCreated) {
            createAdapter(model, baseElement, frameContext);
        }
        if (!mCreated) {
            // This should not happen; indicates a problem in the logic if createAdapter here runs
            // without successfully creating the adapter.
            throw new PietFatalException(ErrorCode.ERR_UNSPECIFIED,
                    frameContext.reportMessage(MessageType.ERROR, "Binding uncreated adapter"));
        }

        onBindModel(model, baseElement, frameContext);
        // Sometimes, onBindModel will short-circuit and set the visibility to GONE (ex. when an
        // optional binding is not found). In that case, we don't want to continue binding.
        if (getView().getVisibility() == View.GONE) {
            return;
        }

        bindActions(frameContext);

        // Reapply styles if we have style bindings
        if (getElementStyleIdsStack().hasStyleBinding()) {
            mElementStyle.applyElementStyles(this);
        }
        for (AccessibilityRole role : baseElement.getAccessibility().getRolesList()) {
            switch (role) {
                case HEADER:
                    getBaseView().setImportantForAccessibility(
                            View.IMPORTANT_FOR_ACCESSIBILITY_YES);
                    getBaseView().setFocusable(true);
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                        getBaseView().setAccessibilityDelegate(new View.AccessibilityDelegate() {
                            @Override
                            public void onInitializeAccessibilityNodeInfo(
                                    View host, AccessibilityNodeInfo info) {
                                super.onInitializeAccessibilityNodeInfo(host, info);
                                info.setHeading(true);
                            }
                        });
                    } else {
                        getBaseView().setAccessibilityDelegate(new View.AccessibilityDelegate() {
                            @Override
                            public void onInitializeAccessibilityNodeInfo(
                                    View host, AccessibilityNodeInfo info) {
                                super.onInitializeAccessibilityNodeInfo(host, info);
                                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
                                    info.setCollectionItemInfo(
                                            AccessibilityNodeInfo.CollectionItemInfo.obtain(
                                                    -1, 0, 0, 0, true));
                                }
                            }
                        });
                    }
                    break;
                default:
                    // Do nothing
            }
        }

        CharSequence contentDescription =
                getAccessibilityString(baseElement.getAccessibility(), frameContext);
        if (contentDescription.length() > 0) {
            getBaseView().setContentDescription(contentDescription);
        } else {
            getBaseView().setContentDescription(null);
        }
        LogDataCallback callback = mParameters.mHostProviders.getLogDataCallback();
        if (callback != null) {
            switch (baseElement.getLogDataValueCase()) {
                case LOG_DATA:
                    mLogData = baseElement.getLogData();
                    break;
                case LOG_DATA_REF:
                    // Get the logData from the binding in the template
                    mLogData = frameContext.getLogDataFromBinding(baseElement.getLogDataRef());
                    break;
                default: // No log Data
            }
            if (mLogData != null) {
                callback.onBind(mLogData, getView());
            }
        }
    }

    private CharSequence getAccessibilityString(
            Accessibility accessibility, FrameContext frameContext) {
        switch (accessibility.getDescriptionDataCase()) {
            case DESCRIPTION:
                return getTemplatedStringEvaluator().evaluate(
                        mParameters.mHostProviders.getAssetProvider(),
                        accessibility.getDescription());
            case DESCRIPTION_BINDING:
                BindingValue bindingValue = frameContext.getParameterizedTextBindingValue(
                        accessibility.getDescriptionBinding());
                if (bindingValue.hasParameterizedText()) {
                    return getTemplatedStringEvaluator().evaluate(
                            mParameters.mHostProviders.getAssetProvider(),
                            bindingValue.getParameterizedText());
                }
                // fall through
            default:
                return "";
        }
    }

    /** Extracts the relevant model from the Element oneof. Throws IllegalArgument if not found. */
    abstract M getModelFromElement(Element baseElement);

    /** Performs child-adapter-specific binding logic; to be overridden. */
    void onBindModel(M model, Element baseElement, FrameContext frameContext) {}

    void bindActions(FrameContext frameContext) {
        // Set up actions. On the server, they are defined either as the actual action, or an
        // "actions_binding". That actions_binding is defined in a template, and the binding value
        // is then looked up in the template invocation.
        switch (mBaseElement.getActionsDataCase()) {
            case ACTIONS:
                mActions = mBaseElement.getActions();
                ViewUtils.setOnClickActions(
                        mActions, getBaseView(), frameContext, mBaseElement.getLogData());
                break;
            case ACTIONS_BINDING:
                // Get the actions from the actions_binding in the template
                mActions = frameContext.getActionsFromBinding(mBaseElement.getActionsBinding());
                ViewUtils.setOnClickActions(
                        mActions, getBaseView(), frameContext, mBaseElement.getLogData());
                break;
            default: // No actions defined.
                mActions = Actions.getDefaultInstance();
                ViewUtils.clearOnClickActions(getBaseView());
                ViewUtils.clearOnLongClickActions(getBaseView());
        }
        setHideActionsActive();
    }

    /** Intended to be called in onCreate when view is offscreen; sets hide actions to active. */
    void setHideActionsActive() {
        mActiveVisibilityActions.clear();
        // Set all "hide" actions as active, since the view is currently off screen.
        mActiveVisibilityActions.addAll(mActions.getOnHideActionsList());
    }

    private static void initializeOverflow(Element baseElement) {
        switch (baseElement.getHorizontalOverflow()) {
            case OVERFLOW_HIDDEN:
            case OVERFLOW_UNSPECIFIED:
                // These are the default behavior
                break;
            default:
                // TODO: Use the standard Piet error codes: ERR_UNSUPPORTED_FEATURE
                Logger.w(TAG, "Unsupported overflow behavior: %s",
                        baseElement.getHorizontalOverflow());
        }
    }

    /**
     * Gets the style of the Element's content (ex. TextElement). Only valid after model has been
     * bound.
     */
    StyleProvider getElementStyle() {
        if (mModel == null) {
            Logger.wtf(TAG, "getElementStyle called when adapter is not bound");
        }
        return mElementStyle;
    }

    /** Returns any styles associated with the bound Element. */
    StyleIdsStack getElementStyleIdsStack() {
        return mBaseElement.getStyleReferences();
    }

    /**
     * Unbinds the current Model from the Adapter. The Adapter will unbind all contained Adapters
     * recursively. Do not override; override {@link #onUnbindModel} instead.
     */
    public void unbindModel() {
        // If the visibility was GONE, we may not have ever called onBindModel.
        if (mCreated) {
            onUnbindModel();
        }
        LogDataCallback callback = mParameters.mHostProviders.getLogDataCallback();
        if (callback != null && mLogData != null) {
            callback.onUnbind(mLogData, getView());
            mLogData = null;
        }
        mModel = null;
        mBaseElement = Element.getDefaultInstance();
        mCheckBoundVisibility = false;

        // Unset actions
        ViewUtils.clearOnLongClickActions(getBaseView());
        ViewUtils.clearOnClickActions(getBaseView());
    }

    /** Performs child-adapter-specific unbind logic; to be overridden. */
    void onUnbindModel() {}

    /**
     * Releases all child adapters to recycler pools. Do not override; override {@link
     * #onReleaseAdapter} instead.
     */
    public void releaseAdapter() {
        if (!mCreated) {
            return;
        }

        onReleaseAdapter();

        // Destroy overlays
        if (mWrapperView != null) {
            mWrapperView.removeAllViews();
            mWrapperView = null;
        }

        setVisibilityOnView(Visibility.VISIBLE);

        mWidthPx = DIMENSION_NOT_SET;
        mHeightPx = DIMENSION_NOT_SET;

        mCreated = false;
    }

    /** Performs child-adapter-specific release logic; to be overridden. */
    void onReleaseAdapter() {}

    /**
     * Subclasses will call this method when they are first bound to a Model to set the Key based
     * upon the Model type.
     */
    void setKey(RecyclerKey key) {
        this.mKey = key;
    }

    /** Returns a {@link RecyclerKey} which represents an instance of a Model based upon a type. */
    @Nullable
    public RecyclerKey getKey() {
        // There are Adapters which don't hold on to their views.  How do we want to mark them?
        return mKey;
    }

    /**
     * Returns the desired width of the adapter, computed from the model at bind time, or {@link
     * StyleProvider#DIMENSION_NOT_SET} which implies that the parent can choose the width.
     */
    int getComputedWidthPx() {
        return mWidthPx;
    }

    /**
     * Returns the desired height of the adapter, computed from the model at bind time, or {@link
     * StyleProvider#DIMENSION_NOT_SET} which implies that the parent can choose the height.
     */
    int getComputedHeightPx() {
        return mHeightPx;
    }

    /** Sets the adapter's desired dimensions based on the style. */
    private void setDesiredDimensions() {
        mWidthPx = mElementStyle.getWidthSpecPx(mContext);
        mHeightPx = mElementStyle.getHeightSpecPx(mContext);
    }

    /** Sets the layout parameters on this adapter's view. */
    public void setLayoutParams(LayoutParams layoutParams) {
        View view = getView();
        if (view == null) {
            return;
        }
        view.setLayoutParams(layoutParams);
        View baseView = getBaseView();
        if (baseView != null && baseView != view) {
            int height = layoutParams.height == LayoutParams.WRAP_CONTENT
                    ? LayoutParams.WRAP_CONTENT
                    : LayoutParams.MATCH_PARENT;
            int width = layoutParams.width == LayoutParams.WRAP_CONTENT ? LayoutParams.WRAP_CONTENT
                                                                        : LayoutParams.MATCH_PARENT;
            baseView.setLayoutParams(new FrameLayout.LayoutParams(width, height));
        }
    }

    AdapterParameters getParameters() {
        return mParameters;
    }

    Context getContext() {
        return mContext;
    }

    /** Returns the {@code model}, but is only legal to call when the Adapter is bound to a model */
    M getModel() {
        return checkNotNull(mModel);
    }

    /** Returns the {@code model}, or null if the adapter is not bound. */
    @Nullable
    M getRawModel() {
        return mModel;
    }

    /** Return the Element's desired vertical placement within its container. */
    int getVerticalGravity(int defaultGravity) {
        if (mModel == null) {
            return defaultGravity;
        }
        return getElementStyle().getGravityVertical(defaultGravity);
    }

    /** Return the Element's desired horizontal placement within its container. */
    int getHorizontalGravity(int defaultGravity) {
        if (mModel == null) {
            return defaultGravity;
        }
        return getElementStyle().getGravityHorizontal(defaultGravity);
    }

    /** Return the Element's desired horizontal and vertical placement within its container. */
    int getGravity(int defaultGravity) {
        if (mModel == null) {
            return defaultGravity;
        }
        return getElementStyle().getGravity(defaultGravity);
    }

    ParameterizedTextEvaluator getTemplatedStringEvaluator() {
        return mParameters.mTemplatedStringEvaluator;
    }

    @VisibleForTesting
    Visibility getVisibilityForElement(Element element, FrameContext frameContext) {
        VisibilityState visibilityState = element.getVisibilityState();
        if (mCheckBoundVisibility && visibilityState.hasOverridingBoundVisibility()) {
            @Nullable
            Visibility overridingVisibility = frameContext.getVisibilityFromBinding(
                    visibilityState.getOverridingBoundVisibility());
            if (overridingVisibility != null) {
                return overridingVisibility;
            }
        }
        return visibilityState.getDefaultVisibility();
    }

    void setVisibilityOnView(Visibility visibility) {
        setVisibilityOnView(visibility, getView());
        if (getView() != getBaseView()) {
            setVisibilityOnView(visibility, getBaseView());
        }
    }

    private static void setVisibilityOnView(Visibility visibility, View view) {
        switch (visibility) {
            case INVISIBLE:
                view.setVisibility(View.INVISIBLE);
                break;
            case GONE:
                view.setVisibility(View.GONE);
                break;
            case VISIBILITY_UNSPECIFIED:
            case VISIBLE:
                view.setVisibility(View.VISIBLE);
                break;
        }
    }

    void triggerHideActions(FrameContext frameContext) {
        ViewUtils.triggerHideActions(getView(), mActions, frameContext.getActionHandler(),
                frameContext.getFrame(), mActiveVisibilityActions);
    }

    void triggerViewActions(View viewport, FrameContext frameContext) {
        ViewUtils.maybeTriggerViewActions(getView(), viewport, mActions,
                frameContext.getActionHandler(), frameContext.getFrame(), mActiveVisibilityActions);
    }
}
