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
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ListView;
import android.widget.Spinner;
import android.widget.TextView;

import org.chromium.android_webview.common.DeveloperModeUtils;
import org.chromium.android_webview.common.Flag;
import org.chromium.android_webview.common.ProductionSupportedFlagList;
import org.chromium.android_webview.common.services.IDeveloperUiService;
import org.chromium.android_webview.common.services.ServiceNames;
import org.chromium.base.Log;

import java.util.HashMap;
import java.util.Map;

/**
 * A fragment to toggle experimental WebView flags/features.
 */
@SuppressLint("SetTextI18n")
public class FlagsFragment extends DevUiBaseFragment {
    private static final String TAG = "WebViewDevTools";

    private static final String STATE_DEFAULT = "Default";
    private static final String STATE_ENABLED = "Enabled";
    private static final String STATE_DISABLED = "Disabled";
    private static final String[] sFlagStates = {
            STATE_DEFAULT,
            STATE_ENABLED,
            STATE_DISABLED,
    };

    private Map<String, Boolean> mOverriddenFlags = new HashMap<>();
    private FlagsListAdapter mListAdapter;

    private Context mContext;

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mContext = context;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_flags, null);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        Activity activity = (Activity) mContext;
        activity.setTitle("WebView Flags");

        ListView flagsListView = view.findViewById(R.id.flags_list);
        TextView flagsDescriptionView = view.findViewById(R.id.flags_description);
        flagsDescriptionView.setText("By enabling these features, you could "
                + "lose app data or compromise your security or privacy. Enabled features apply to "
                + "WebViews across all apps on the device.");

        // Restore flag overrides from the service process to repopulate the UI, if developer mode
        // is enabled.
        if (DeveloperModeUtils.isDeveloperModeEnabled(mContext.getPackageName())) {
            mOverriddenFlags = DeveloperModeUtils.getFlagOverrides(mContext.getPackageName());
        }

        mListAdapter = new FlagsListAdapter(sortFlagList(ProductionSupportedFlagList.sFlagList));
        flagsListView.setAdapter(mListAdapter);

        Button resetFlagsButton = view.findViewById(R.id.reset_flags_button);
        resetFlagsButton.setOnClickListener((View flagButton) -> { resetAllFlags(); });
    }

    /**
     * Sorts the flag list so enabled/disabled flags are at the beginning and default flags are at
     * the end.
     */
    private Flag[] sortFlagList(Flag[] unsorted) {
        Flag[] sortedFlags = new Flag[unsorted.length];
        int i = 0;
        for (Flag flag : unsorted) {
            if (mOverriddenFlags.containsKey(flag.getName())) {
                sortedFlags[i++] = flag;
            }
        }
        for (Flag flag : unsorted) {
            if (!mOverriddenFlags.containsKey(flag.getName())) {
                sortedFlags[i++] = flag;
            }
        }
        assert sortedFlags.length == unsorted.length : "arrays should be same length";
        return sortedFlags;
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
            int newState = position;

            switch (sFlagStates[newState]) {
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

            // Update UI and Service. Only communicate with the service if the map actually updated.
            // This optimizes the number of IPCs we make, but this also allows for atomic batch
            // updates by updating mOverriddenFlags prior to updating the Spinner state.
            if (oldState != newState) {
                sendFlagsToService();

                ViewParent grandparent = parent.getParent();
                if (grandparent instanceof View) {
                    formatListEntry((View) grandparent, newState);
                }
            }
        }

        @Override
        public void onNothingSelected(AdapterView<?> parent) {}
    }

    /**
     * Adapter to create rows of toggleable Flags.
     */
    private class FlagsListAdapter extends ArrayAdapter<Flag> {
        public FlagsListAdapter(Flag[] sortedFlags) {
            super(mContext, R.layout.toggleable_flag, sortedFlags);
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
            String label = flag.getName();
            if (flag.getEnabledStateValue() != null) {
                label += "=" + flag.getEnabledStateValue();
            }
            flagName.setText(label);
            flagDescription.setText(flag.getDescription());
            ArrayAdapter<String> adapter =
                    new ArrayAdapter<>(mContext, R.layout.flag_states, sFlagStates);
            adapter.setDropDownViewResource(android.R.layout.select_dialog_singlechoice);
            flagToggle.setAdapter(adapter);

            // Populate spinner state from map and update indicators.
            int state = booleanToState(mOverriddenFlags.get(flag.getName()));
            flagToggle.setSelection(state);
            flagToggle.setOnItemSelectedListener(new FlagStateSpinnerSelectedListener(flag));
            formatListEntry(view, state);

            return view;
        }
    }

    /**
     * Formats a flag list entry. {@code toggleableFlag} should be the View which holds the {@link
     * Spinner}, flag title, flag description, etc. as children.
     *
     * @param toggleableFlag a View representing an entire flag entry.
     * @param state the state of the flag.
     */
    private void formatListEntry(View toggleableFlag, int state) {
        TextView flagName = toggleableFlag.findViewById(R.id.flag_name);
        if (state == /* STATE_DEFAULT */ 0) {
            // Unset the compound drawable.
            flagName.setCompoundDrawablesWithIntrinsicBounds(0, 0, 0, 0);
        } else { // STATE_ENABLED or STATE_DISABLED
            // Draws a blue circle to the left of the text.
            flagName.setCompoundDrawablesWithIntrinsicBounds(R.drawable.blue_circle, 0, 0, 0);
        }
    }

    private class FlagsServiceConnection implements ServiceConnection {
        public void start() {
            Intent intent = new Intent();
            intent.setClassName(mContext.getPackageName(), ServiceNames.DEVELOPER_UI_SERVICE);
            if (!mContext.bindService(intent, this, Context.BIND_AUTO_CREATE)) {
                Log.e(TAG, "Failed to bind to Developer UI service");
            }
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            try {
                IDeveloperUiService.Stub.asInterface(service).setFlagOverrides(mOverriddenFlags);
            } catch (RemoteException e) {
                Log.e(TAG, "Failed to send flag overrides to service", e);
            } finally {
                // Unbind when we've sent the flags overrides, since we can always rebind later. The
                // service will manage its own lifetime.
                mContext.unbindService(this);
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {}
    }

    private void sendFlagsToService() {
        FlagsServiceConnection connection = new FlagsServiceConnection();
        connection.start();
    }

    private void resetAllFlags() {
        // Clear the map, then update the Spinners from the map value.
        mOverriddenFlags.clear();
        mListAdapter.notifyDataSetChanged();
        sendFlagsToService();
    }
}
