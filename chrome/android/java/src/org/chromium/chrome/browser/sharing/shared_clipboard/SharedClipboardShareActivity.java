// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing.shared_clipboard;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.sharing.SharingAdapter;
import org.chromium.chrome.browser.sharing.SharingDeviceCapability;
import org.chromium.chrome.browser.sharing.SharingNotificationUtil;
import org.chromium.chrome.browser.sharing.SharingSendMessageResult;
import org.chromium.chrome.browser.sharing.SharingServiceProxy;
import org.chromium.chrome.browser.sharing.SharingServiceProxy.DeviceInfo;

/**
 * Activity to display device targets to share text.
 */
public class SharedClipboardShareActivity
        extends AsyncInitializationActivity implements OnItemClickListener {
    private SharingAdapter mAdapter;
    private ListView mListView;

    /**
     * Checks whether sending shared clipboard message is enabled for the user and enables/disables
     * the SharedClipboardShareActivity appropriately. This call requires native to be loaded.
     */
    public static void updateComponentEnabledState() {
        boolean enabled = ChromeFeatureList.isEnabled(ChromeFeatureList.SHARED_CLIPBOARD_UI);
        PostTask.postTask(TaskTraits.USER_VISIBLE, () -> setComponentEnabled(enabled));
    }

    /**
     * Sets whether or not the SharedClipboardShareActivity should be enabled. This may trigger a
     * StrictMode violation so shouldn't be called on the UI thread.
     */
    private static void setComponentEnabled(boolean enabled) {
        ThreadUtils.assertOnBackgroundThread();
        Context context = ContextUtils.getApplicationContext();
        PackageManager packageManager = context.getPackageManager();
        ComponentName componentName =
                new ComponentName(context, SharedClipboardShareActivity.class);

        int newState = enabled ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                               : PackageManager.COMPONENT_ENABLED_STATE_DISABLED;

        // This indicates that we don't want to kill Chrome when changing component enabled state.
        int flags = PackageManager.DONT_KILL_APP;

        if (packageManager.getComponentEnabledSetting(componentName) != newState) {
            packageManager.setComponentEnabledSetting(componentName, newState, flags);
        }
    }

    @Override
    protected void triggerLayoutInflation() {
        setContentView(R.layout.sharing_device_picker);

        View mask = findViewById(R.id.mask);
        mask.setOnClickListener(v -> finish());

        TextView toolbarText = findViewById(R.id.device_picker_toolbar);
        toolbarText.setText(R.string.send_tab_to_self_sheet_toolbar);

        mListView = findViewById(R.id.device_picker_list);
        mListView.setAdapter(mAdapter);
        mListView.setOnItemClickListener(this);
        mListView.setEmptyView(findViewById(android.R.id.empty));

        onInitialLayoutInflationComplete();
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();

        mAdapter = new SharingAdapter(SharingDeviceCapability.SHARED_CLIPBOARD);
        mListView.setAdapter(mAdapter);
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return false;
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        DeviceInfo device = mAdapter.getItem(position);

        String text = getIntent().getStringExtra(Intent.EXTRA_TEXT);

        int token = SharingNotificationUtil.showSendingNotification(
                NotificationUmaTracker.SystemNotificationType.SHARED_CLIPBOARD,
                NotificationConstants.GROUP_SHARED_CLIPBOARD,
                NotificationConstants.NOTIFICATION_ID_SHARED_CLIPBOARD_OUTGOING, device.clientName);

        SharingServiceProxy.getInstance().sendSharedClipboardMessage(device.guid, text, result -> {
            if (result == SharingSendMessageResult.SUCCESSFUL) {
                SharingNotificationUtil.dismissNotification(
                        NotificationConstants.GROUP_SHARED_CLIPBOARD,
                        NotificationConstants.NOTIFICATION_ID_SHARED_CLIPBOARD_OUTGOING);
            } else {
                String contentTitle = getErrorNotificationTitle(result);
                String contentText = getErrorNotificationText(result, device.clientName);

                SharingNotificationUtil.showSendErrorNotification(
                        NotificationUmaTracker.SystemNotificationType.SHARED_CLIPBOARD,
                        NotificationConstants.GROUP_SHARED_CLIPBOARD,
                        NotificationConstants.NOTIFICATION_ID_SHARED_CLIPBOARD_OUTGOING,
                        contentTitle, contentText, token);
            }
        });
        finish();
    }

    @Override
    public void finish() {
        super.finish();
        // TODO(alexchau): Handle animations.
        overridePendingTransition(R.anim.no_anim, R.anim.no_anim);
    }

    /**
     * Return the title of error notification shown based on result of send message to other device.
     * TODO(himanshujaju) - All text except PAYLOAD_TOO_LARGE are common across features. Extract
     * them out when next feature is added.
     *
     * @param result    The result of sending message to other device.
     * @return the title for error notification.
     */
    private String getErrorNotificationTitle(@SharingSendMessageResult int result) {
        Resources resources = ContextUtils.getApplicationContext().getResources();
        String contentType = resources.getString(R.string.browser_sharing_content_type_text);

        switch (result) {
            case SharingSendMessageResult.DEVICE_NOT_FOUND:
            case SharingSendMessageResult.NETWORK_ERROR:
            case SharingSendMessageResult.ACK_TIMEOUT:
                return resources.getString(
                        R.string.browser_sharing_error_dialog_title_generic_error, contentType);

            case SharingSendMessageResult.PAYLOAD_TOO_LARGE:
                return resources.getString(
                        R.string.browser_sharing_shared_clipboard_error_dialog_title_payload_too_large);

            case SharingSendMessageResult.INTERNAL_ERROR:
                return resources.getString(
                        R.string.browser_sharing_error_dialog_title_internal_error, contentType);

            default:
                assert false;
                return resources.getString(
                        R.string.browser_sharing_error_dialog_title_internal_error, contentType);
        }
    }

    /**
     * Return the text of error notification shown based on result of send message to other device.
     * TODO(himanshujaju) - All text except PAYLOAD_TOO_LARGE are common across features. Extract
     * them out when next feature is added.
     *
     * @param result    The result of sending message to other device.
     * @return the text for error notification.
     */
    private String getErrorNotificationText(
            @SharingSendMessageResult int result, String targetDeviceName) {
        Resources resources = ContextUtils.getApplicationContext().getResources();

        switch (result) {
            case SharingSendMessageResult.DEVICE_NOT_FOUND:
                return resources.getString(
                        R.string.browser_sharing_error_dialog_text_device_not_found,
                        targetDeviceName);

            case SharingSendMessageResult.NETWORK_ERROR:
                return resources.getString(
                        R.string.browser_sharing_error_dialog_text_network_error);

            case SharingSendMessageResult.PAYLOAD_TOO_LARGE:
                return resources.getString(
                        R.string.browser_sharing_shared_clipboard_error_dialog_text_payload_too_large);

            case SharingSendMessageResult.ACK_TIMEOUT:
                return resources.getString(
                        R.string.browser_sharing_error_dialog_text_device_ack_timeout,
                        targetDeviceName);

            case SharingSendMessageResult.INTERNAL_ERROR:
                return resources.getString(
                        R.string.browser_sharing_error_dialog_text_internal_error);

            default:
                assert false;
                return resources.getString(
                        R.string.browser_sharing_error_dialog_text_internal_error);
        }
    }
}
