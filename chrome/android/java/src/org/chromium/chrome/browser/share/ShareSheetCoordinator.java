// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.Context;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.send_tab_to_self.SendTabToSelfShareActivity;
import org.chromium.chrome.browser.share.qrcode.QrCodeCoordinator;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * Coordinator for displaying the share sheet.
 */
public class ShareSheetCoordinator {
    private static final int SHARE_SHEET_ITEM = 0;

    private final BottomSheetController mBottomSheetController;
    private final ActivityTabProvider mActivityTabProvider;

    /**
     * Constructs a new ShareSheetCoordinator.
     */
    public ShareSheetCoordinator(BottomSheetController controller, ActivityTabProvider provider) {
        mBottomSheetController = controller;
        mActivityTabProvider = provider;
    }

    protected void showShareSheet(ShareParams params) {
        Context context = params.getWindow().getContext().get();
        ShareSheetBottomSheetContent bottomSheet = new ShareSheetBottomSheetContent(context);

        // QR Codes
        PropertyModel qrcodePropertyModel =
                new PropertyModel.Builder(ShareSheetItemViewProperties.ALL_KEYS)
                        .with(ShareSheetItemViewProperties.ICON,
                                AppCompatResources.getDrawable(context, R.drawable.qr_code))
                        .with(ShareSheetItemViewProperties.LABEL,
                                context.getResources().getString(R.string.qr_code_share_icon_label))
                        .with(ShareSheetItemViewProperties.CLICK_LISTENER,
                                (currentContext) -> {
                                    QrCodeCoordinator qrCodeCoordinator =
                                            new QrCodeCoordinator(context);
                                    qrCodeCoordinator.show();
                                })
                        .build();

        // Send Tab To Self
        PropertyModel sttsPropertyModel =
                new PropertyModel.Builder(ShareSheetItemViewProperties.ALL_KEYS)
                        .with(ShareSheetItemViewProperties.ICON,
                                AppCompatResources.getDrawable(context, R.drawable.send_tab))
                        .with(ShareSheetItemViewProperties.LABEL,
                                context.getResources().getString(
                                        R.string.send_tab_to_self_share_activity_title))
                        .with(ShareSheetItemViewProperties.CLICK_LISTENER,
                                (shareParams) -> {
                                    mBottomSheetController.hideContent(bottomSheet, true);
                                    SendTabToSelfShareActivity.actionHandler(
                                            params.getWindow().getContext().get(),
                                            mActivityTabProvider.get()
                                                    .getWebContents()
                                                    .getNavigationController()
                                                    .getVisibleEntry(),
                                            mBottomSheetController);
                                })
                        .build();

        ModelList modelList = new ModelList();
        modelList.add(new ListItem(SHARE_SHEET_ITEM, qrcodePropertyModel));
        modelList.add(new ListItem(SHARE_SHEET_ITEM, sttsPropertyModel));
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(modelList);
        RecyclerView rcView =
                bottomSheet.getContentView().findViewById(R.id.share_sheet_chrome_apps);
        adapter.registerType(SHARE_SHEET_ITEM, () -> {
            return (ViewGroup) LayoutInflater.from(context).inflate(
                    R.layout.share_sheet_item, (ViewGroup) rcView, false);
        }, ShareSheetCoordinator::bindShareItem);

        LinearLayoutManager layoutManager =
                new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false);
        rcView.setLayoutManager(layoutManager);
        rcView.setAdapter(adapter);

        mBottomSheetController.requestShowContent(bottomSheet, true);
    }

    private static void bindShareItem(
            PropertyModel model, ViewGroup parent, PropertyKey propertyKey) {
        if (ShareSheetItemViewProperties.ICON.equals(propertyKey)) {
            ImageView view = (ImageView) parent.findViewById(R.id.icon);
            view.setImageDrawable(model.get(ShareSheetItemViewProperties.ICON));
        } else if (ShareSheetItemViewProperties.LABEL.equals(propertyKey)) {
            TextView view = (TextView) parent.findViewById(R.id.text);
            view.setText(model.get(ShareSheetItemViewProperties.LABEL));
        } else if (ShareSheetItemViewProperties.CLICK_LISTENER.equals(propertyKey)) {
            parent.setOnClickListener(model.get(ShareSheetItemViewProperties.CLICK_LISTENER));
        }
    }
}
