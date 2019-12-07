// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkState;
import static org.chromium.chrome.browser.feed.library.piet.StyleProvider.DIMENSION_NOT_SET;
import static org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode.ERR_GRID_CELL_WIDTH_WITHOUT_CONTENTS;
import static org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode.ERR_MISSING_ELEMENT_CONTENTS;
import static org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode.ERR_UNSUPPORTED_FEATURE;

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import android.view.Gravity;
import android.view.View;
import android.widget.LinearLayout;

import org.chromium.base.Supplier;
import org.chromium.chrome.browser.feed.library.common.ui.LayoutUtils;
import org.chromium.chrome.browser.feed.library.piet.AdapterFactory.AdapterKeySupplier;
import org.chromium.chrome.browser.feed.library.piet.AdapterFactory.SingletonKeySupplier;
import org.chromium.chrome.browser.feed.library.piet.DebugLogger.MessageType;
import org.chromium.chrome.browser.feed.library.piet.ui.GridRowView;
import org.chromium.chrome.browser.feed.library.piet.ui.GridRowView.LayoutParams;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Content;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.Element;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.GridCell;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.GridCellWidth;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.GridCellWidth.WidthSpecCase;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.GridRow;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** An {@link ElementContainerAdapter} which manages {@code GridRow} slices. */
class GridRowAdapter extends ElementContainerAdapter<GridRowView, GridRow> {
    private static final String TAG = "GridRowAdapter";

    private GridRowAdapter(Context context, AdapterParameters parameters) {
        super(context, parameters,
                createView(context, parameters.mHostProviders.getAssetProvider().isRtLSupplier()),
                KeySupplier.SINGLETON_KEY);
    }

    @Override
    GridRow getModelFromElement(Element baseElement) {
        if (!baseElement.hasGridRow()) {
            throw new PietFatalException(ERR_MISSING_ELEMENT_CONTENTS,
                    String.format("Missing GridRow; has %s", baseElement.getElementsCase()));
        }
        return baseElement.getGridRow();
    }

    @Override
    List<Content> getContentsFromModel(GridRow model) {
        ArrayList<Content> contents = new ArrayList<>();
        for (GridCell cell : model.getCellsList()) {
            contents.add(cell.getContent());
        }
        contents.trimToSize();
        return Collections.unmodifiableList(contents);
    }

    @Override
    void onBindModel(GridRow gridRow, Element baseElement, FrameContext frameContext) {
        super.onBindModel(gridRow, baseElement, frameContext);

        int adapterIndex = 0;
        checkState(gridRow.getCellsCount() == mAdaptersPerContent.length,
                "Mismatch between number of cells (%s) and adaptersPerContent (%s);"
                        + " problem in creation?",
                gridRow.getCellsCount(), mAdaptersPerContent.length);
        for (int contentIndex = 0; contentIndex < gridRow.getCellsCount(); contentIndex++) {
            GridCell cell = gridRow.getCells(contentIndex);
            for (int i = 0; i < mAdaptersPerContent[contentIndex]; i++) {
                setLayoutParamsOnCell(mChildAdapters.get(adapterIndex), cell, frameContext);
                adapterIndex++;
            }
        }
    }

    @SuppressWarnings("UnnecessaryDefaultInEnumSwitch")
    private void setLayoutParamsOnCell(ElementAdapter<? extends View, ?> cellAdapter, GridCell cell,
            FrameContext frameContext) {
        // TODO: Support full-cell backgrounds and horizontal/vertical gravity for all
        // types of ElementAdapter in GridCell.

        GridCellWidth gridCellWidth = null;
        if (cell.hasWidth()) {
            gridCellWidth = cell.getWidth();
        } else if (cell.hasWidthBinding()) {
            gridCellWidth = frameContext.getGridCellWidthFromBinding(cell.getWidthBinding());
        }

        // Default cell is weight = 1 and height = WRAP_CONTENT.
        boolean isCollapsible = gridCellWidth != null
                && gridCellWidth.getWidthSpecCase() != WidthSpecCase.WEIGHT
                && gridCellWidth.getIsCollapsible();
        LayoutParams params;
        if (isCollapsible) {
            params = new LayoutParams(
                    LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT, 0.0f, true);
        } else {
            params = new LayoutParams(0, LayoutParams.WRAP_CONTENT, 1.0f, false);
        }

        // If a width is specified on the proto, we expect it to be populated so we can use it.
        if (gridCellWidth != null) {
            switch (gridCellWidth.getWidthSpecCase()) {
                case CONTENT_WIDTH:
                    params.weight = 0;
                    params.width = LayoutParams.WRAP_CONTENT;
                    switch (gridCellWidth.getContentWidth()) {
                        case CONTENT_WIDTH:
                            if (cellAdapter.getComputedWidthPx() == LayoutParams.MATCH_PARENT) {
                                frameContext.reportMessage(MessageType.WARNING,
                                        ERR_UNSUPPORTED_FEATURE,
                                        "FIT_PARENT width is invalid for CONTENT_WIDTH"
                                                + " cell contents.");
                            } else if (cellAdapter.getComputedWidthPx() != DIMENSION_NOT_SET) {
                                params.width = cellAdapter.getComputedWidthPx();
                            }
                            break;
                        case INVALID_CONTENT_WIDTH:
                        default:
                            frameContext.reportMessage(MessageType.WARNING,
                                    ERR_GRID_CELL_WIDTH_WITHOUT_CONTENTS,
                                    String.format("Invalid content width: %s",
                                            gridCellWidth.getContentWidth().name()));
                    }
                    break;
                case DP:
                    params.weight = 0;
                    params.width = (int) LayoutUtils.dpToPx(gridCellWidth.getDp(), getContext());
                    break;
                case WEIGHT:
                    params.weight = gridCellWidth.getWeight();
                    params.width = 0;
                    break;
                case WIDTHSPEC_NOT_SET:
                default:
                    frameContext.reportMessage(MessageType.WARNING,
                            ERR_GRID_CELL_WIDTH_WITHOUT_CONTENTS,
                            String.format("Invalid content width: %s",
                                    gridCellWidth.getContentWidth().name()));
            }
        }

        params.height = cellAdapter.getComputedHeightPx() == DIMENSION_NOT_SET
                ? LayoutParams.MATCH_PARENT
                : cellAdapter.getComputedHeightPx();

        cellAdapter.getElementStyle().applyMargins(getContext(), params);

        params.gravity = cellAdapter.getVerticalGravity(Gravity.TOP);

        cellAdapter.setLayoutParams(params);
    }

    @VisibleForTesting
    static GridRowView createView(Context context, Supplier<Boolean> isRtL) {
        GridRowView viewGroup = new GridRowView(context, isRtL);
        viewGroup.setOrientation(LinearLayout.HORIZONTAL);
        viewGroup.setLayoutParams(new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        viewGroup.setBaselineAligned(false);
        viewGroup.setClipToPadding(false);
        return viewGroup;
    }

    /** A {@link AdapterKeySupplier} for the {@link GridRowAdapter}. */
    static class KeySupplier extends SingletonKeySupplier<GridRowAdapter, GridRow> {
        @Override
        public String getAdapterTag() {
            return TAG;
        }

        @Override
        public GridRowAdapter getAdapter(Context context, AdapterParameters parameters) {
            return new GridRowAdapter(context, parameters);
        }
    }
}
