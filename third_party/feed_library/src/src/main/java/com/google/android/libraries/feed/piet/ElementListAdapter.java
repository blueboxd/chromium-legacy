// Copyright 2018 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.android.libraries.feed.piet;

import static com.google.android.libraries.feed.piet.StyleProvider.DIMENSION_NOT_SET;

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import com.google.android.libraries.feed.piet.AdapterFactory.SingletonKeySupplier;
import com.google.android.libraries.feed.piet.DebugLogger.MessageType;
import com.google.search.now.ui.piet.ElementsProto.Content;
import com.google.search.now.ui.piet.ElementsProto.Element;
import com.google.search.now.ui.piet.ElementsProto.ElementList;
import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;
import java.util.List;

/** An {@link ElementContainerAdapter} which manages vertical lists of elements. */
class ElementListAdapter extends ElementContainerAdapter<LinearLayout, ElementList> {
  private static final String TAG = "ElementListAdapter";

  // Only needed for reporting errors during updateChildLayoutParams.
  /*@Nullable*/ private FrameContext frameContextForDebugLogsFromCreate = null;
  /*@Nullable*/ private FrameContext frameContextForDebugLogsFromBind = null;

  private ElementListAdapter(Context context, AdapterParameters parameters) {
    super(context, parameters, createView(context), KeySupplier.SINGLETON_KEY);
  }

  @Override
  ElementList getModelFromElement(Element baseElement) {
    if (!baseElement.hasElementList()) {
      throw new PietFatalException(
          ErrorCode.ERR_MISSING_ELEMENT_CONTENTS,
          String.format("Missing ElementList; has %s", baseElement.getElementsCase()));
    }
    return baseElement.getElementList();
  }

  @Override
  void onCreateAdapter(ElementList model, Element baseElement, FrameContext frameContext) {
    this.frameContextForDebugLogsFromCreate = frameContext;
    super.onCreateAdapter(model, baseElement, frameContext);
  }

  @Override
  void onBindModel(ElementList model, Element baseElement, FrameContext frameContext) {
    this.frameContextForDebugLogsFromBind = frameContext;
    super.onBindModel(model, baseElement, frameContext);
    for (ElementAdapter<? extends View, ?> adapter : childAdapters) {
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
    this.frameContextForDebugLogsFromBind = null;
  }

  @Override
  void onReleaseAdapter() {
    super.onReleaseAdapter();
    this.frameContextForDebugLogsFromCreate = null;
  }

  @Override
  public void setLayoutParams(ViewGroup.LayoutParams layoutParams) {
    super.setLayoutParams(layoutParams);
    for (ElementAdapter<? extends View, ?> adapter : childAdapters) {
      updateChildLayoutParams(adapter);
    }
  }

  /*@Nullable*/
  private FrameContext getLoggingFrameContext() {
    return frameContextForDebugLogsFromBind != null
        ? frameContextForDebugLogsFromBind
        : frameContextForDebugLogsFromCreate;
  }

  private void updateChildLayoutParams(ElementAdapter<? extends View, ?> adapter) {
    LayoutParams childParams = new LayoutParams(0, 0);

    int childHeight = adapter.getComputedHeightPx();
    if (childHeight == DIMENSION_NOT_SET) {
      childHeight = LayoutParams.WRAP_CONTENT;
    } else if (childHeight == LayoutParams.MATCH_PARENT) {
      FrameContext loggingFrameContext = getLoggingFrameContext();
      if (loggingFrameContext != null) {
        loggingFrameContext.reportMessage(
            MessageType.WARNING,
            ErrorCode.ERR_UNSUPPORTED_FEATURE,
            "FILL_PARENT not supported for height on ElementList contents.");
      }
      childHeight = LayoutParams.WRAP_CONTENT;
    }

    childParams.width =
        adapter.getComputedWidthPx() == DIMENSION_NOT_SET
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
