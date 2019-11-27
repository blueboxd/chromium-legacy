// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static org.chromium.chrome.browser.feed.library.piet.StyleProvider.DIMENSION_NOT_SET;

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import org.chromium.chrome.browser.feed.library.piet.AdapterFactory.SingletonKeySupplier;
import org.chromium.chrome.browser.feed.library.piet.DebugLogger.MessageType;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Content;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.ElementList;
import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;

import java.util.List;

/** An {@link ElementContainerAdapter} which manages vertical lists of elements. */
class ElementListAdapter extends ElementContainerAdapter<LinearLayout, ElementList> {
    private static final String TAG = "ElementListAdapter";

    // Only needed for reporting errors during updateChildLayoutParams.
    /*@Nullable*/ private FrameContext mFrameContextForDebugLogsFromCreate;
    /*@Nullable*/ private FrameContext mFrameContextForDebugLogsFromBind;

    private ElementListAdapter(Context context, AdapterParameters parameters) {
        super(context, parameters, createView(context), KeySupplier.SINGLETON_KEY);
    }

    @Override
    ElementList getModelFromElement(Element baseElement) {
        if (!baseElement.hasElementList()) {
            throw new PietFatalException(ErrorCode.ERR_MISSING_ELEMENT_CONTENTS,
                    String.format("Missing ElementList; has %s", baseElement.getElementsCase()));
        }
        return baseElement.getElementList();
    }

    @Override
    void onCreateAdapter(ElementList model, Element baseElement, FrameContext frameContext) {
        this.mFrameContextForDebugLogsFromCreate = frameContext;
        super.onCreateAdapter(model, baseElement, frameContext);
    }

    @Override
    void onBindModel(ElementList model, Element baseElement, FrameContext frameContext) {
        this.mFrameContextForDebugLogsFromBind = frameContext;
        super.onBindModel(model, baseElement, frameContext);
        for (ElementAdapter<? extends View, ?> adapter : mChildAdapters) {
            updateChildLayoutParams(adapter);
        }
    }

    @Override
    List<Content> getContentsFromModel(ElementList model) {
        return model.getContentsList();
    }

    @Override
    void onUnbindModel() {
        super.onUnbindModel();
        this.mFrameContextForDebugLogsFromBind = null;
    }

    @Override
    void onReleaseAdapter() {
        super.onReleaseAdapter();
        this.mFrameContextForDebugLogsFromCreate = null;
    }

    @Override
    public void setLayoutParams(ViewGroup.LayoutParams layoutParams) {
        super.setLayoutParams(layoutParams);
        for (ElementAdapter<? extends View, ?> adapter : mChildAdapters) {
            updateChildLayoutParams(adapter);
        }
    }

    /*@Nullable*/
    private FrameContext getLoggingFrameContext() {
        return mFrameContextForDebugLogsFromBind != null ? mFrameContextForDebugLogsFromBind
                                                         : mFrameContextForDebugLogsFromCreate;
    }

    private void updateChildLayoutParams(ElementAdapter<? extends View, ?> adapter) {
        LayoutParams childParams = new LayoutParams(0, 0);

        int childHeight = adapter.getComputedHeightPx();
        if (childHeight == DIMENSION_NOT_SET) {
            childHeight = LayoutParams.WRAP_CONTENT;
        } else if (childHeight == LayoutParams.MATCH_PARENT) {
            FrameContext loggingFrameContext = getLoggingFrameContext();
            if (loggingFrameContext != null) {
                loggingFrameContext.reportMessage(MessageType.WARNING,
                        ErrorCode.ERR_UNSUPPORTED_FEATURE,
                        "FILL_PARENT not supported for height on ElementList contents.");
            }
            childHeight = LayoutParams.WRAP_CONTENT;
        }

        childParams.width = adapter.getComputedWidthPx() == DIMENSION_NOT_SET
                ? LayoutParams.MATCH_PARENT
                : adapter.getComputedWidthPx();
        childParams.height = childHeight;

        adapter.getElementStyle().applyMargins(getContext(), childParams);

        childParams.gravity = adapter.getHorizontalGravity(Gravity.START);

        adapter.setLayoutParams(childParams);
    }

    @VisibleForTesting
    static LinearLayout createView(Context context) {
        LinearLayout viewGroup = new LinearLayout(context);
        viewGroup.setOrientation(LinearLayout.VERTICAL);
        viewGroup.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        viewGroup.setClipToPadding(false);
        return viewGroup;
    }

    static class KeySupplier extends SingletonKeySupplier<ElementListAdapter, ElementList> {
        @Override
        public String getAdapterTag() {
            return TAG;
        }

        @Override
        public ElementListAdapter getAdapter(Context context, AdapterParameters parameters) {
            return new ElementListAdapter(context, parameters);
        }
    }
}
