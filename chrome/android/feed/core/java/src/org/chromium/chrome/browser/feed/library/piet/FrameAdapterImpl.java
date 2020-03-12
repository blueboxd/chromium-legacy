// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkNotNull;
import static org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode.ERR_MISSING_OR_UNHANDLED_CONTENT;
import static org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode.ERR_POOR_FRAME_RATE;

import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feed.library.api.host.config.DebugBehavior;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.piet.DebugLogger.MessageType;
import org.chromium.chrome.browser.feed.library.piet.TemplateBinder.TemplateAdapterModel;
import org.chromium.chrome.browser.feed.library.piet.host.ActionHandler;
import org.chromium.chrome.browser.feed.library.piet.host.EventLogger;
import org.chromium.chrome.browser.feed.library.piet.host.LogDataCallback;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.VisibilityAction;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.BindingContext;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Content;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.TemplateInvocation;
import org.chromium.components.feed.core.proto.ui.piet.PietAndroidSupport.ShardingControl;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Frame;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.PietSharedState;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * An adapter which manages {@link Frame} instances. Frames will contain one or more slices. This
 * class has additional public methods to support host access to the primary view of the frame
 * before the model is bound to the frame. A frame is basically a vertical LinearLayout of slice
 * Views which are created by {@link ElementAdapter}. This Adapter is not created through a Factory
 * and is managed by the host.
 */
public class FrameAdapterImpl implements FrameAdapter {
    private static final String TAG = "FrameAdapter";

    private static final String GENERIC_EXCEPTION = "Top Level Exception was caught - see logcat";

    private final Set<ElementAdapter<?, ?>> mChildAdapters;

    private final Context mContext;
    private final AdapterParameters mParameters;
    private final ActionHandler mActionHandler;
    private final EventLogger mEventLogger;
    private final DebugBehavior mDebugBehavior;
    private final Set<VisibilityAction> mActiveActions = new HashSet<>();
    @Nullable
    private LinearLayout mView;
    @Nullable
    private FrameContext mFrameContext;

    FrameAdapterImpl(Context context, AdapterParameters parameters, ActionHandler actionHandler,
            EventLogger eventLogger, DebugBehavior debugBehavior) {
        this.mContext = context;
        this.mParameters = parameters;
        this.mActionHandler = actionHandler;
        this.mEventLogger = eventLogger;
        this.mDebugBehavior = debugBehavior;
        mChildAdapters = new HashSet<>();
    }

    // TODO: Need to implement support for sharding
    @Override
    public void bindModel(Frame frame, int frameWidthPx, @Nullable ShardingControl shardingControl,
            List<PietSharedState> pietSharedStates) {
        long startTime = System.nanoTime();
        initialBind(mParameters.mParentViewSupplier.get());
        FrameContext localFrameContext =
                createFrameContext(frame, frameWidthPx, pietSharedStates, checkNotNull(mView));
        mFrameContext = localFrameContext;
        mActiveActions.clear();
        mActiveActions.addAll(frame.getActions().getOnHideActionsList());
        LinearLayout frameView = checkNotNull(mView);

        try {
            for (Content content : frame.getContentsList()) {
                // For Slice we will create the lower level slice instead to remove the extra
                // level.
                List<ElementAdapter<?, ?>> adapters =
                        getBoundAdaptersForContent(content, localFrameContext);
                for (ElementAdapter<?, ?> adapter : adapters) {
                    mChildAdapters.add(adapter);
                    setLayoutParamsOnChild(adapter);
                    frameView.addView(adapter.getView());
                }
            }

            StyleProvider style = localFrameContext.makeStyleFor(frame.getStyleReferences());
            frameView.setBackground(style.createBackground());
        } catch (RuntimeException e) {
            // TODO: Remove this once error reporting is fully implemented.
            Logger.e(TAG, e, "Catch top level exception");
            String message = e.getMessage() != null ? e.getMessage() : GENERIC_EXCEPTION;
            if (e instanceof PietFatalException) {
                localFrameContext.reportMessage(
                        MessageType.ERROR, ((PietFatalException) e).getErrorCode(), message);
            } else {
                localFrameContext.reportMessage(MessageType.ERROR, message);
            }
        }
        startTime = System.nanoTime() - startTime;
        // TODO: We should be targeting < 15ms and warn at 10ms?
        //   Until we get a chance to do the performance tuning, leave this at 30ms to prevent
        //   warnings on large GridRows based frames.
        if (startTime / 1000000 > 30) {
            Logger.w(TAG,
                    localFrameContext.reportMessage(MessageType.WARNING, ERR_POOR_FRAME_RATE,
                            String.format("Slow Bind (%s) time: %s ps", frame.getTag(),
                                    startTime / 1000)));
        }
        // If there were errors add an error slice to the frame
        if (localFrameContext.getDebugBehavior().getShowDebugViews()) {
            View errorView =
                    localFrameContext.getDebugLogger().getReportView(MessageType.ERROR, mContext);
            if (errorView != null) {
                frameView.addView(errorView);
            }
            View warningView =
                    localFrameContext.getDebugLogger().getReportView(MessageType.WARNING, mContext);
            if (warningView != null) {
                frameView.addView(warningView);
            }
        }
        mEventLogger.logEvents(localFrameContext.getDebugLogger().getErrorCodes());
        LogDataCallback callback = mParameters.mHostProviders.getLogDataCallback();
        Frame localFrame = localFrameContext.getFrame();
        if (callback != null && localFrame != null && localFrame.hasLogData()) {
            callback.onBind(localFrame.getLogData(), frameView);
        }
    }

    @Override
    public void unbindModel() {
        triggerHideActions();

        LinearLayout view = checkNotNull(this.mView);
        LogDataCallback callback = mParameters.mHostProviders.getLogDataCallback();
        if (mFrameContext != null) {
            Frame frame = mFrameContext.getFrame();
            if (callback != null && frame != null && frame.hasLogData()) {
                callback.onUnbind(frame.getLogData(), view);
            }
        }
        for (ElementAdapter<?, ?> child : mChildAdapters) {
            mParameters.mElementAdapterFactory.releaseAdapter(child);
        }
        mChildAdapters.clear();
        view.removeAllViews();
        mFrameContext = null;
    }

    private void setLayoutParamsOnChild(ElementAdapter<?, ?> childAdapter) {
        int width = childAdapter.getComputedWidthPx();
        width = width == StyleProvider.DIMENSION_NOT_SET ? LayoutParams.MATCH_PARENT : width;
        int height = childAdapter.getComputedHeightPx();
        height = height == StyleProvider.DIMENSION_NOT_SET ? LayoutParams.WRAP_CONTENT : height;

        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(width, height);

        childAdapter.getElementStyle().applyMargins(mContext, params);

        params.gravity = childAdapter.getGravity(Gravity.TOP | Gravity.START);

        childAdapter.setLayoutParams(params);
    }

    @Override
    public LinearLayout getFrameContainer() {
        initialBind(mParameters.mParentViewSupplier.get());
        return checkNotNull(mView);
    }

    @VisibleForTesting
    FrameContext createFrameContext(
            Frame frame, int frameWidthPx, List<PietSharedState> pietSharedStates, View frameView) {
        MediaQueryHelper mediaQueryHelper = new MediaQueryHelper(
                frameWidthPx, mParameters.mHostProviders.getAssetProvider(), mContext);
        PietStylesHelper pietStylesHelper = checkNotNull(
                mParameters.mPietStylesHelperFactory.get(pietSharedStates, mediaQueryHelper));
        return FrameContext.createFrameContext(frame, pietSharedStates, pietStylesHelper,
                mDebugBehavior, new DebugLogger(), mActionHandler, mParameters.mHostProviders,
                frameView);
    }

    @Override
    public void triggerHideActions() {
        if (mFrameContext == null || mView == null) {
            return;
        }
        FrameContext localFrameContext = mFrameContext;
        ViewUtils.triggerHideActions(mView, localFrameContext.getFrame().getActions(),
                localFrameContext.getActionHandler(), localFrameContext.getFrame(), mActiveActions);

        for (ElementAdapter<?, ?> adapter : mChildAdapters) {
            adapter.triggerHideActions(localFrameContext);
        }
    }

    @Override
    public void triggerViewActions(View viewport) {
        if (mFrameContext == null || mView == null) {
            return;
        }
        FrameContext localFrameContext = mFrameContext;
        ViewUtils.maybeTriggerViewActions(mView, viewport,
                localFrameContext.getFrame().getActions(), localFrameContext.getActionHandler(),
                localFrameContext.getFrame(), mActiveActions);

        for (ElementAdapter<?, ?> adapter : mChildAdapters) {
            adapter.triggerViewActions(viewport, localFrameContext);
        }
    }

    @VisibleForTesting
    List<ElementAdapter<?, ?>> getBoundAdaptersForContent(
            Content content, FrameContext frameContext) {
        switch (content.getContentTypeCase()) {
            case ELEMENT:
                Element element = content.getElement();
                ElementAdapter<?, ?> inlineSliceAdapter =
                        mParameters.mElementAdapterFactory.createAdapterForElement(
                                element, frameContext);
                inlineSliceAdapter.bindModel(element, frameContext);
                return Collections.singletonList(inlineSliceAdapter);
            case TEMPLATE_INVOCATION:
                TemplateInvocation templateInvocation = content.getTemplateInvocation();
                ArrayList<ElementAdapter<?, ?>> returnList = new ArrayList<>();
                for (BindingContext bindingContext : templateInvocation.getBindingContextsList()) {
                    TemplateAdapterModel model = new TemplateAdapterModel(
                            templateInvocation.getTemplateId(), frameContext, bindingContext);
                    ElementAdapter<? extends View, ?> templateAdapter =
                            mParameters.mTemplateBinder.createAndBindTemplateAdapter(
                                    model, frameContext);
                    returnList.add(templateAdapter);
                }
                returnList.trimToSize();
                return Collections.unmodifiableList(returnList);
            default:
                Logger.wtf(TAG,
                        frameContext.reportMessage(MessageType.ERROR,
                                ERR_MISSING_OR_UNHANDLED_CONTENT,
                                String.format("Unsupported Content type for frame: %s",
                                        content.getContentTypeCase())));
                return Collections.emptyList();
        }
    }

    @VisibleForTesting
    AdapterParameters getParameters() {
        return this.mParameters;
    }

    @VisibleForTesting
    @Nullable
    LinearLayout getView() {
        return this.mView;
    }

    private void initialBind(@Nullable ViewGroup parent) {
        if (mView != null) {
            return;
        }
        this.mView = createView(parent);
    }

    private LinearLayout createView(@Nullable ViewGroup parent) {
        LinearLayout linearLayout = new LinearLayout(mContext);
        linearLayout.setOrientation(LinearLayout.VERTICAL);
        ViewGroup.LayoutParams layoutParams;
        if (parent != null && parent.getLayoutParams() != null) {
            layoutParams = new LinearLayout.LayoutParams(parent.getLayoutParams());
            layoutParams.width = LayoutParams.MATCH_PARENT;
            layoutParams.height = LayoutParams.WRAP_CONTENT;
        } else {
            layoutParams = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
        }
        linearLayout.setLayoutParams(layoutParams);
        return linearLayout;
    }
}
