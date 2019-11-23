// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.database.DataSetObserver;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseExpandableListAdapter;
import android.widget.Button;
import android.widget.ExpandableListView;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.WorkerThread;

import org.chromium.android_webview.common.CommandLineUtil;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.common.crash.CrashInfo;
import org.chromium.android_webview.common.crash.CrashInfo.UploadState;
import org.chromium.android_webview.devui.util.NavigationMenuHelper;
import org.chromium.android_webview.devui.util.WebViewCrashInfoCollector;
import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.task.AsyncTask;

import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/**
 * An activity to show a list of recent WebView crashes.
 */
public class CrashesListActivity extends Activity {
    private static final String TAG = "WebViewDevTools";

    // Max number of crashes to show in the crashes list.
    private static final int MAX_CRASHES_NUMBER = 20;

    private CrashListExpandableAdapter mCrashListViewAdapter;
    private WebViewPackageError mDifferentPackageError;

    private static final String CRASH_REPORT_TEMPLATE = ""
            + "IMPORTANT: Your crash has already been automatically reported to our crash system. "
            + "You only need to fill this out if you can share more information like steps to "
            + "reproduce the crash.\n"
            + "\n"
            + "Device name:\n"
            + "Android OS version:\n"
            + "WebView version (On Android L-M, this is the version of the 'Android System "
            + "WebView' app. On Android N-P, it's most likely Chrome's version. You can find the "
            + "version of any app under Settings > Apps > the 3 dots in the upper right > Show "
            + "system.):\n"
            + "Application: (Please link to its Play Store page if possible. You can get the link "
            + "from inside the Play Store app by tapping the 3 dots in the upper right > Share > "
            + "Copy to clipboard. Or you can find the app on the Play Store website: "
            + "https://play.google.com/store/apps.)\n"
            + "Application version:\n"
            + "\n"
            + "\n"
            + "Steps to reproduce:\n"
            + "(1)\n"
            + "(2)\n"
            + "(3)\n"
            + "\n"
            + "\n"
            + "Expected result:\n"
            + "(What should have happened?)\n"
            + "\n"
            + "\n"
            + "****DO NOT CHANGE BELOW THIS LINE****\n"
            + "Crash ID: http://crash/%s\n";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_crashes_list);

        mCrashListViewAdapter = new CrashListExpandableAdapter();
        ExpandableListView crashListView = findViewById(R.id.crashes_list);
        crashListView.setAdapter(mCrashListViewAdapter);

        mDifferentPackageError =
                new WebViewPackageError(this, findViewById(R.id.crashes_list_activity_layout));
        // show the dialog once when the activity is created.
        mDifferentPackageError.showDialogIfDifferent();

        final boolean enableMinidumpUploadingForTesting = CommandLine.getInstance().hasSwitch(
                CommandLineUtil.CRASH_UPLOADS_ENABLED_FOR_TESTING_SWITCH);
        if (!enableMinidumpUploadingForTesting) {
            PlatformServiceBridge.getInstance().queryMetricsSetting(enabled -> {
                // enabled is a Boolean object and can be null.
                if (!Boolean.TRUE.equals(enabled)) {
                    showCrashConsentError(PlatformServiceBridge.getInstance().canUseGms());
                }
            });
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        // Check package status in onResume() to hide/show the error message if the user
        // changes WebView implementation from system settings and then returns back to the
        // activity.
        mDifferentPackageError.showMessageIfDifferent();
        mCrashListViewAdapter.updateCrashes();
    }

    /**
     * Adapter to create crashes list items from a list of CrashInfo.
     */
    private class CrashListExpandableAdapter extends BaseExpandableListAdapter {
        private List<CrashInfo> mCrashInfoList;

        CrashListExpandableAdapter() {
            mCrashInfoList = new ArrayList<>();

            TextView crashesSummaryView = findViewById(R.id.crashes_summary_textview);
            // Update crash summary when the data changes.
            registerDataSetObserver(new DataSetObserver() {
                @Override
                public void onChanged() {
                    crashesSummaryView.setText(
                            String.format(Locale.US, "Crashes (%d)", mCrashInfoList.size()));
                }
            });
        }

        // Group View which is used as header for a crash in crashes list.
        // We show:
        //   - Icon of the app where the crash happened.
        //   - Package name of the app where the crash happened.
        //   - Time when the crash happened.
        @Override
        public View getGroupView(
                int groupPosition, boolean isExpanded, View view, ViewGroup parent) {
            // If the the old view is already created then reuse it, else create a new one by layout
            // inflation.
            if (view == null) {
                view = getLayoutInflater().inflate(R.layout.crashes_list_item_header, null);
            }

            CrashInfo crashInfo = (CrashInfo) getGroup(groupPosition);

            ImageView packageIcon = view.findViewById(R.id.crash_package_icon);
            String packageName;
            if (crashInfo.packageName == null) {
                // This can happen if crash log file where we keep crash info is cleared but other
                // log files like upload logs still exist.
                packageName = "unknown app";
                packageIcon.setImageResource(android.R.drawable.sym_def_app_icon);
            } else {
                packageName = crashInfo.packageName;
                try {
                    Drawable icon = getPackageManager().getApplicationIcon(packageName);
                    packageIcon.setImageDrawable(icon);
                } catch (PackageManager.NameNotFoundException e) {
                    // This can happen if the app was uninstalled after the crash was recorded.
                    packageIcon.setImageResource(android.R.drawable.sym_def_app_icon);
                }
            }
            setTwoLineListItemText(view.findViewById(R.id.crash_header), packageName,
                    new Date(crashInfo.captureTime).toString());

            return view;
        }

        // Child View where more info about the crash is shown:
        //    - Variation keys for the crash.
        //    - Crash report upload status.
        @Override
        public View getChildView(int groupPosition, final int childPosition, boolean isLastChild,
                View view, ViewGroup parent) {
            // If the the old view is already created then reuse it, else create a new one by layout
            // inflation.
            if (view == null) {
                view = getLayoutInflater().inflate(R.layout.crashes_list_item_body, null);
            }

            CrashInfo crashInfo = (CrashInfo) getChild(groupPosition, childPosition);
            // Variations keys
            setTwoLineListItemText(view.findViewById(R.id.variations), "Variations",
                    crashInfo.variations == null ? "Not available"
                                                 : crashInfo.variations.toString());
            // Upload info
            String uploadState = uploadStateString(crashInfo.uploadState);
            String uploadInfo = null;
            if (crashInfo.uploadState == UploadState.UPLOADED) {
                uploadInfo = new Date(crashInfo.uploadTime).toString();
                uploadInfo += "\nID: " + crashInfo.uploadId;
            }
            setTwoLineListItemText(view.findViewById(R.id.upload_status), uploadState, uploadInfo);

            Button button = view.findViewById(R.id.crash_report_button);
            // Report button is only clickable if the crash report is uploaded.
            if (crashInfo.uploadState == UploadState.UPLOADED) {
                button.setEnabled(true);
                button.setOnClickListener(v -> {
                    startActivity(new Intent(Intent.ACTION_VIEW, getReportUri(crashInfo)));
                });
            } else {
                button.setEnabled(false);
            }

            return view;
        }

        @Override
        public boolean isChildSelectable(int groupPosition, int childPosition) {
            return true;
        }

        @Override
        public Object getGroup(int groupPosition) {
            return mCrashInfoList.get(groupPosition);
        }

        @Override
        public Object getChild(int groupPosition, int childPosition) {
            return mCrashInfoList.get(groupPosition);
        }

        @Override
        public long getGroupId(int groupPosition) {
            // Hash code of local id is unique per crash info object.
            return ((CrashInfo) getGroup(groupPosition)).localId.hashCode();
        }

        @Override
        public long getChildId(int groupPosition, int childPosition) {
            // Child ID refers to a piece of info in a particular crash report. It is stable
            // since we don't change the order we show this info in runtime and currently we show
            // all information in only one child item.
            return childPosition;
        }

        @Override
        public boolean hasStableIds() {
            // Stable IDs mean both getGroupId and getChildId return stable IDs for each group and
            // child i.e: an ID always refers to the same object. See getGroupId and getChildId for
            // why the IDs are stable.
            return true;
        }

        @Override
        public int getChildrenCount(int groupPosition) {
            // Crash info is shown in one child item.
            return 1;
        }

        @Override
        public int getGroupCount() {
            return mCrashInfoList.size();
        }

        /**
         * Asynchronously load crash info on a background thread and then update the UI when the
         * data is loaded.
         */
        public void updateCrashes() {
            AsyncTask<List<CrashInfo>> asyncTask = new AsyncTask<List<CrashInfo>>() {
                @Override
                @WorkerThread
                protected List<CrashInfo> doInBackground() {
                    WebViewCrashInfoCollector crashCollector = new WebViewCrashInfoCollector();
                    return crashCollector.loadCrashesInfo(MAX_CRASHES_NUMBER);
                }

                @Override
                protected void onPostExecute(List<CrashInfo> result) {
                    mCrashInfoList = result;
                    notifyDataSetChanged();
                }
            };
            asyncTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }
    }

    // Build a report uri to open an issue on https://bugs.chromium.org/p/chromium/issues/entry.
    // It uses WebView Bugs Template and adds "User-Submitted" Label.
    // It adds the upload id at the end of the template and populates the Application package
    // name field.
    // TODO(https://crbug.com/991594) populate more fields in the template.
    private static Uri getReportUri(CrashInfo crashInfo) {
        return new Uri.Builder()
                .scheme("https")
                .authority("bugs.chromium.org")
                .path("/p/chromium/issues/entry")
                .appendQueryParameter("template", "Webview+Bugs")
                .appendQueryParameter("comment",
                        String.format(Locale.US, CRASH_REPORT_TEMPLATE, crashInfo.uploadId))
                .appendQueryParameter("labels", "User-Submitted")
                .build();
    }

    private static String uploadStateString(UploadState uploadState) {
        switch (uploadState) {
            case UPLOADED:
                return "Uploaded";
            case PENDING:
            case PENDING_USER_REQUESTED:
                return "Pending upload";
            case SKIPPED:
                return "Skipped upload";
        }
        return null;
    }

    // Helper method to find and set text for two line list item. If a null String is passed, the
    // relevant TextView will be hidden.
    private static void setTwoLineListItemText(
            @NonNull View view, @Nullable String title, @Nullable String subtitle) {
        TextView titleView = view.findViewById(android.R.id.text1);
        TextView subtitleView = view.findViewById(android.R.id.text2);
        if (titleView != null) {
            titleView.setVisibility(View.VISIBLE);
            titleView.setText(title);
        } else {
            titleView.setVisibility(View.GONE);
        }
        if (subtitle != null) {
            subtitleView.setVisibility(View.VISIBLE);
            subtitleView.setText(subtitle);
        } else {
            subtitleView.setVisibility(View.GONE);
        }
    }

    private void showCrashConsentError(boolean canUseGms) {
        AlertDialog.Builder dialogBuilder = new AlertDialog.Builder(this);
        dialogBuilder.setTitle("Error Showing Crashes");
        if (canUseGms) {
            dialogBuilder.setMessage(
                    "Crash collection is disabled. Please turn on 'Usage & diagnostics' "
                    + "to enable WebView crash collection.");
            // Open Google Settings activity, "Usage & diagnostics" activity is not exported and
            // cannot be opened directly.
            Intent settingsIntent = new Intent("com.android.settings.action.EXTRA_SETTINGS");
            List<ResolveInfo> intentResolveInfo =
                    getPackageManager().queryIntentActivities(settingsIntent, 0);
            // Show a button to open GMS settings activity only if it exists.
            if (intentResolveInfo.size() > 0) {
                dialogBuilder.setPositiveButton(
                        "Settings", (dialog, id) -> startActivity(settingsIntent));
            } else {
                Log.e(TAG, "Cannot find GMS settings activity");
            }
        } else {
            dialogBuilder.setMessage("Crash collection is not supported at the moment.");
        }

        new PersistentErrorView(this, PersistentErrorView.Type.ERROR)
                .prependToLinearLayout(findViewById(R.id.crashes_list_activity_layout))
                .setText("Crash collection is disabled. Tap for more info.")
                .setDialog(dialogBuilder.create())
                .show();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.crashes_options_menu, menu);
        NavigationMenuHelper.inflate(this, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (NavigationMenuHelper.onOptionsItemSelected(this, item)) {
            return true;
        }
        if (item.getItemId() == R.id.options_menu_refresh) {
            mCrashListViewAdapter.updateCrashes();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }
}
