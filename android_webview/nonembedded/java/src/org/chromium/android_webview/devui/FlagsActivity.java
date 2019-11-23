// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ListView;
import android.widget.Spinner;
import android.widget.TextView;

import org.chromium.android_webview.common.Flag;
import org.chromium.android_webview.common.ProductionSupportedFlagList;
import org.chromium.android_webview.common.services.IDeveloperUiService;
import org.chromium.android_webview.common.services.ServiceNames;
import org.chromium.android_webview.devui.util.NavigationMenuHelper;
import org.chromium.base.Log;

import java.util.HashMap;
import java.util.Map;

/**
 * An activity to toggle experimental WebView flags/features.
 */
@SuppressLint("SetTextI18n")
public class FlagsActivity extends Activity {
    private static final String TAG = "WebViewDevTools";

    private static final String STATE_DEFAULT = "Default";
    private static final String STATE_ENABLED = "Enabled";
    private static final String STATE_DISABLED = "Disabled";
    private static final String[] sFlagStates = {
            STATE_DEFAULT,
            STATE_ENABLED,
            STATE_DISABLED,
    };

    private WebViewPackageError mDifferentPackageError;
    private final Map<String, Boolean> mOverriddenFlags = new HashMap<>();
    private FlagsListAdapter mListAdapter;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_flags);

        ListView flagsListView = findViewById(R.id.flags_list);
        TextView flagsDescriptionView = findViewById(R.id.flags_description);
        flagsDescriptionView.setText("By enabling these features, you could "
                + "lose app data or compromise your security or privacy. Enabled features apply to "
                + "WebViews across all apps on the device.");

        mListAdapter = new FlagsListAdapter();
        flagsListView.setAdapter(mListAdapter);

        Button resetFlagsButton = findViewById(R.id.reset_flags_button);
        resetFlagsButton.setOnClickListener((View view) -> { resetAllFlags(); });

        mDifferentPackageError =
                new WebViewPackageError(this, findViewById(R.id.flags_activity_layout));
        // show the dialog once when the activity is created.
        mDifferentPackageError.showDialogIfDifferent();

        // TODO(ntfschr): once there's a way to get the flag overrides out of the service, we should
        // repopulate the UI based on that data (otherwise, we send an empty map to the service,
        // which causes the service to stop itself).
    }

    @Override
    protected void onResume() {
        super.onResume();
        // Check package status in onResume() to hide/show the error message if the user
        // changes WebView implementation from system settings and then returns back to the
        // activity.
        mDifferentPackageError.showMessageIfDifferent();
    }

    private static int booleanToState(Boolean b) {
        if (b == null) {
            return /* STATE_DEFAULT */ 0;
        } else if (b) {
            return /* STATE_ENABLED */ 1;
        }
        return /* STATE_DISABLED */ 2;
    }

    private class FlagStateSpinnerSelectedListener implements AdapterView.OnItemSelectedListener {
        private Flag mFlag;

        FlagStateSpinnerSelectedListener(Flag flag) {
            mFlag = flag;
        }

        @Override
        public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
            String flagName = mFlag.getName();
            int oldState = booleanToState(mOverriddenFlags.get(flagName));

            switch (sFlagStates[position]) {
                case STATE_DEFAULT:
                    mOverriddenFlags.remove(flagName);
                    break;
                case STATE_ENABLED:
                    mOverriddenFlags.put(flagName, true);
                    break;
                case STATE_DISABLED:
                    mOverriddenFlags.put(flagName, false);
                    break;
            }

            // Only communicate with the service if the map actually updated. This optimizes the
            // number of IPCs we make, but this also allows for atomic batch updates by updating
            // mOverriddenFlags prior to updating the Spinner state.
            int newState = booleanToState(mOverriddenFlags.get(flagName));
            if (oldState != newState) {
                sendFlagsToService();
            }
        }

        @Override
        public void onNothingSelected(AdapterView<?> parent) {}
    }

    /**
     * Adapter to create rows of toggleable Flags.
     */
    private class FlagsListAdapter extends ArrayAdapter<Flag> {
        public FlagsListAdapter() {
            super(FlagsActivity.this, R.layout.toggleable_flag,
                    ProductionSupportedFlagList.sFlagList);
        }

        @Override
        public View getView(int position, View view, ViewGroup parent) {
            // If the the old view is already created then reuse it, else create a new one by layout
            // inflation.
            if (view == null) {
                view = getLayoutInflater().inflate(R.layout.toggleable_flag, null);
            }

            TextView flagName = view.findViewById(R.id.flag_name);
            TextView flagDescription = view.findViewById(R.id.flag_description);
            Spinner flagToggle = view.findViewById(R.id.flag_toggle);

            Flag flag = getItem(position);
            flagName.setText(flag.getName());
            flagDescription.setText(flag.getDescription());
            ArrayAdapter<String> adapter =
                    new ArrayAdapter<>(FlagsActivity.this, R.layout.flag_states, sFlagStates);
            adapter.setDropDownViewResource(android.R.layout.select_dialog_singlechoice);
            flagToggle.setAdapter(adapter);

            // Populate spinner state from map.
            int state = booleanToState(mOverriddenFlags.get(flag.getName()));
            flagToggle.setSelection(state);
            flagToggle.setOnItemSelectedListener(new FlagStateSpinnerSelectedListener(flag));

            return view;
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        NavigationMenuHelper.inflate(this, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (NavigationMenuHelper.onOptionsItemSelected(this, item)) {
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private class FlagsServiceConnection implements ServiceConnection {
        public void start() {
            enableDeveloperMode();
            Intent intent = new Intent();
            intent.setClassName(
                    FlagsActivity.this.getPackageName(), ServiceNames.DEVELOPER_UI_SERVICE);
            if (!FlagsActivity.this.bindService(intent, this, Context.BIND_AUTO_CREATE)) {
                Log.e(TAG, "Failed to bind to Developer UI service");
            }
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            Intent intent = new Intent();
            intent.setClassName(
                    FlagsActivity.this.getPackageName(), ServiceNames.DEVELOPER_UI_SERVICE);
            try {
                IDeveloperUiService.Stub.asInterface(service).setFlagOverrides(mOverriddenFlags);
            } catch (RemoteException e) {
                Log.e(TAG, "Failed to send flag overrides to service", e);
            } finally {
                // Unbind when we've sent the flags overrides, since we can always rebind later. The
                // service will manage its own lifetime.
                FlagsActivity.this.unbindService(this);
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {}
    }

    private void sendFlagsToService() {
        FlagsServiceConnection connection = new FlagsServiceConnection();
        connection.start();
    }

    private void enableDeveloperMode() {
        ComponentName developerModeService =
                new ComponentName(this, ServiceNames.DEVELOPER_UI_SERVICE);
        this.getPackageManager().setComponentEnabledSetting(developerModeService,
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED, PackageManager.DONT_KILL_APP);
    }

    private void resetAllFlags() {
        // Clear the map, then update the Spinners from the map value.
        mOverriddenFlags.clear();
        mListAdapter.notifyDataSetChanged();
        sendFlagsToService();
    }
}
