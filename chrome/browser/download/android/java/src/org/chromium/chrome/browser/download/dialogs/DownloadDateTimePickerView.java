// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.content.Context;
import android.os.Build;
import android.util.AttributeSet;
import android.widget.DatePicker;
import android.widget.LinearLayout;
import android.widget.TimePicker;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.download.dialogs.DownloadDateTimePickerDialogProperties.State;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Calendar;

/**
 * The view for download date time picker. Contains a {@link DatePicker} and a {@link TimePicker}.
 */
public class DownloadDateTimePickerView extends LinearLayout {
    /**
     * The view binder to propagate events from model to view.
     */
    public static class Binder {
        public static void bind(
                PropertyModel model, DownloadDateTimePickerView view, PropertyKey propertyKey) {
            if (propertyKey == DownloadDateTimePickerDialogProperties.INITIAL_TIME) {
                view.setTime(model.get(DownloadDateTimePickerDialogProperties.INITIAL_TIME));
            } else if (propertyKey == DownloadDateTimePickerDialogProperties.MIN_TIME) {
                view.setMinTime(model.get(DownloadDateTimePickerDialogProperties.MIN_TIME));
            } else if (propertyKey == DownloadDateTimePickerDialogProperties.MAX_TIME) {
                view.setMaxTime(model.get(DownloadDateTimePickerDialogProperties.MAX_TIME));
            } else if (propertyKey == DownloadDateTimePickerDialogProperties.STATE) {
                view.setState(model.get(DownloadDateTimePickerDialogProperties.STATE));
            }
        }
    }

    private DatePicker mDatePicker;
    private TimePicker mTimePicker;

    public DownloadDateTimePickerView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mDatePicker = findViewById(R.id.date_picker);
        mTimePicker = findViewById(R.id.time_picker);
    }

    /**
     * Sets the state of the date time picker dialog.
     * @param state The state of the picker.
     */
    void setState(@State int state) {
        switch (state) {
            case State.DATE:
                // Show the date picker.
                mDatePicker.setVisibility(VISIBLE);
                mTimePicker.setVisibility(GONE);
                break;
            case State.TIME:
                // Show the time picker.
                mDatePicker.setVisibility(GONE);
                mTimePicker.setVisibility(VISIBLE);
                break;
            default:
                assert false;
        }
    }

    /**
     * Sets the initial time shown on the {@link DatePicker} and {@link TimePicker}.
     * @param initialTime The initial time shown on the time picker.
     */
    void setTime(long initialTime) {
        Calendar calendar = Calendar.getInstance();
        calendar.setTimeInMillis(initialTime);
        mDatePicker.updateDate(calendar.get(Calendar.YEAR), calendar.get(Calendar.MONTH),
                calendar.get(Calendar.DAY_OF_MONTH));

        setHour(mTimePicker, calendar.get(Calendar.HOUR));
        setMinute(mTimePicker, calendar.get(Calendar.MINUTE));
    }

    /**
     * Gets the unix timestamp of the selected date and time.
     * @return The selected time.
     */
    public long getTime() {
        Calendar calendar = Calendar.getInstance();
        calendar.set(Calendar.YEAR, mDatePicker.getYear());
        calendar.set(Calendar.MONTH, mDatePicker.getMonth());
        calendar.set(Calendar.DAY_OF_MONTH, mDatePicker.getDayOfMonth());
        calendar.set(Calendar.HOUR, getHour(mTimePicker));
        calendar.set(Calendar.MINUTE, getMinute(mTimePicker));
        return calendar.getTimeInMillis();
    }

    /**
     * Sets the minimum time for the user to select. It is possible for the user to select a time
     * before this when the user select today as the date and a time before the current time. The
     * caller should safely handle this edge case.
     * @param minTime The minimum time as a unix timestamp for the user to select.
     */
    public void setMinTime(long minTime) {
        mDatePicker.setMinDate(minTime);
    }

    /**
     * Sets the maximum time for the user to select. It is possible for the user to select a time
     * after this when the user select the maximum date and a time after the maximum time. The
     * caller should safely handle this edge case, or ignore this since the error will be less than
     * 24 hours.
     * @param maxTime The maximum time as a unix timestamp for the user to select.
     */
    public void setMaxTime(long maxTime) {
        mDatePicker.setMaxDate(maxTime);
    }

    // TODO(xingliu): Move these to a util class and use them for all TimePicker call sites.
    private static int getHour(TimePicker timePicker) {
        assert (timePicker != null);
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return timePicker.getCurrentHour();
        return timePicker.getHour();
    }

    private static int getMinute(TimePicker timePicker) {
        assert (timePicker != null);
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return timePicker.getCurrentMinute();
        return timePicker.getMinute();
    }

    private static void setHour(TimePicker timePicker, int hour) {
        assert (timePicker != null);
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            timePicker.setCurrentHour(hour);
            return;
        }
        timePicker.setHour(hour);
    }

    private static void setMinute(TimePicker timePicker, int minute) {
        assert (timePicker != null);
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            timePicker.setCurrentMinute(minute);
            return;
        }
        timePicker.setMinute(minute);
    }
}
