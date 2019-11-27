// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.logging;

import android.support.annotation.VisibleForTesting;
import android.text.TextUtils;

import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/**
 * Simple Dumper modelled after the AGSA Dumper. This will dump a tree of objects which implement
 * the {@link Dumpable} interface.
 */
public final class Dumper {
    private static final String TAG = "Dumper";
    private static final String SINGLE_INDENT = "  ";

    private final int mIndentLevel;

    // Walk up the chain of parents to
    // ensure none are currently dumping a given dumpable, to prevent cycles. The WeakReference
    // itself is always non-null, but the Dumpable may be null.
    private final WeakReference<Dumpable> mCurrentDumpable;
    /*@Nullable*/ private final Dumper mParent;
    private final boolean mRedacted;

    // The root Dumper will create the values used by all children Dumper instances.  This flattens
    // the output into one list.
    @VisibleForTesting
    final List<DumperValue> mValues;

    /** Returns the default Dumper, this will show sensitive content. */
    public static Dumper newDefaultDumper() {
        return new Dumper(1, null, new WeakReference<>(null), new ArrayList<>(), false);
    }

    /** Returns a Dumper which will redact sensitive content. */
    public static Dumper newRedactedDumper() {
        return new Dumper(1, null, new WeakReference<>(null), new ArrayList<>(), true);
    }

    // Private constructor, these are created through the static newDefaultDumper} method.
    private Dumper(int indentLevel,
            /*@Nullable*/ Dumper parent, WeakReference<Dumpable> currentDumpable,
            List<DumperValue> values, boolean redacted) {
        this.mIndentLevel = indentLevel;
        this.mParent = parent;
        this.mCurrentDumpable = currentDumpable;
        this.mValues = values;
        this.mRedacted = redacted;
    }

    private boolean isDescendentOf(Dumpable dumpable) {
        return (mCurrentDumpable.get() == dumpable)
                || (mParent != null && mParent.isDescendentOf(dumpable));
    }

    /** Creates a new Dumper with a indent level one greater than the current indent level. */
    public Dumper getChildDumper() {
        return getChildDumper(null);
    }

    private Dumper getChildDumper(/*@Nullable*/ Dumpable dumpable) {
        return new Dumper(
                mIndentLevel + 1, this, new WeakReference<>(dumpable), mValues, mRedacted);
    }

    /** Set the title of the section. This is output at the previous indent level. */
    public void title(String title) {
        mValues.add(new DumperValue(mIndentLevel - 1, title));
    }

    /** Adds a String as an indented line to the dump */
    public void format(String format, Object... args) {
        addLine("", String.format(Locale.US, format, args));
    }

    /** Create a Dumper value with the specified name. */
    public DumperValue forKey(String name) {
        return addLine(mIndentLevel, name);
    }

    /** Allow the indent level to be adjusted. */
    public DumperValue forKey(String name, int indentAdjustment) {
        return addLine(mIndentLevel + indentAdjustment, name);
    }

    /** Dump a Dumpable as a child of the current Dumper */
    public void dump(/*@Nullable*/ Dumpable dumpable) {
        if (dumpable == null) {
            return;
        }
        if (isDescendentOf(dumpable)) {
            format("[cycle detected]");
            return;
        }
        Dumper child = getChildDumper(dumpable);
        try {
            dumpable.dump(child);
        } catch (Exception e) {
            Logger.e(TAG, e, "Dump Failed");
        }
    }

    /** Write the Dumpable to an {@link Appendable}. */
    public void write(Appendable writer) throws IOException {
        boolean newLine = true;
        for (DumperValue value : mValues) {
            String stringValue = value.toString(mRedacted);
            if (!newLine) {
                if (value.mCompactPrevious) {
                    writer.append(" | ");
                } else {
                    String indent = getIndent(value.mIndentLevel);
                    writer.append("\n").append(indent);
                }
            }
            writer.append(stringValue);
            newLine = false;
        }
        writer.append("\n");
    }

    private DumperValue addLine(int indentLevel, String title) {
        DumperValue dumperValue = new DumperValue(indentLevel, title);
        mValues.add(dumperValue);
        return dumperValue;
    }

    private DumperValue addLine(String name, String value) {
        return forKey(name).value(value);
    }

    /**
     * Class which represents a name value pair within the dump. It is used for both titles and for
     * the name value pair content within the dump. The indent level is used to specify the indent
     * level for content appearing on a new line. Multiple DumperValues may be compacted into a
     * single line in the output.
     */
    public static final class DumperValue {
        @VisibleForTesting
        static final String REDACTED = "[REDACTED]";
        @VisibleForTesting
        final String mName;
        @VisibleForTesting
        final StringBuilder mContent;
        @VisibleForTesting
        final int mIndentLevel;
        private boolean mCompactPrevious;
        private boolean mSensitive;

        // create a DumpValue with just a name, this is not public, it will is called by Dumper
        DumperValue(int indentLevel, String name) {
            this.mIndentLevel = indentLevel;
            this.mName = name;
            this.mContent = new StringBuilder();
        }

        /** Append an int to the current content of this value */
        public DumperValue value(int value) {
            this.mContent.append(value);
            return this;
        }

        /** Append an String to the current content of this value */
        public DumperValue value(String value) {
            this.mContent.append(value);
            return this;
        }

        /** Append an boolean to the current content of this value */
        public DumperValue value(boolean value) {
            this.mContent.append(value);
            return this;
        }

        /** Add a Date value. It will be formatted as Logcat Dates are formatted */
        public DumperValue value(Date value) {
            this.mContent.append(StringFormattingUtils.formatLogDate(value));
            return this;
        }

        /** Add a value specified as an object. */
        public DumperValue valueObject(Object value) {
            if (value instanceof Integer) {
                return value((Integer) value);
            }
            if (value instanceof Boolean) {
                return value((Boolean) value);
            }
            return value(value.toString());
        }

        /**
         * When output, this DumperValue will be compacted to the same output line as the previous
         * value.
         */
        public DumperValue compactPrevious() {
            mCompactPrevious = true;
            return this;
        }

        /** Mark the Value as containing sensitive data */
        public DumperValue sensitive() {
            mSensitive = true;
            return this;
        }

        /**
         * Output the value as a Name/Value pair. This will convert sensitive data to "REDACTED" if
         * we are not dumping sensitive data.
         */
        String toString(boolean suppressSensitive) {
            String value = "";
            if (!TextUtils.isEmpty(mContent)) {
                value = (suppressSensitive && mSensitive) ? REDACTED : mContent.toString();
            }
            if (!TextUtils.isEmpty(mName) && !TextUtils.isEmpty(value)) {
                return mName + ": " + value;
            } else if (!TextUtils.isEmpty(mName)) {
                return mName + ":";
            } else if (!TextUtils.isEmpty(value)) {
                return value;
            } else {
                return "";
            }
        }
    }

    private static final String[] INDENTATIONS = new String[] {
            "",
            createIndent(1),
            createIndent(2),
            createIndent(3),
            createIndent(4),
            createIndent(5),
    };

    private static String createIndent(int size) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < size; i++) {
            sb.append(SINGLE_INDENT);
        }
        return sb.toString();
    }

    private static String getIndent(int indentLevel) {
        if (indentLevel < 0) {
            return INDENTATIONS[0];
        } else if (indentLevel < INDENTATIONS.length) {
            return INDENTATIONS[indentLevel];
        } else {
            return createIndent(indentLevel);
        }
    }
}
