// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.config;

import android.support.annotation.StringDef;

import java.util.HashMap;

/**
 * Contains an immutable collection of {@link ConfigKey} {@link String}, {@link Object} pairs.
 *
 * <p>Note: this class should not be mocked. Use the {@link Builder} instead.
 */
// TODO: This can't be final because we mock it
public class Configuration {
    /** A unique string identifier for a config value */
    @StringDef({
            // Keep sorted.
            // Configuration to have Stream abort restores if user is past configured fold count.
            ConfigKey.ABANDON_RESTORE_BELOW_FOLD,
            // Configuration on threshold of cards which abandons a restore if scroll is past that
            // content
            // count.
            ConfigKey.ABANDON_RESTORE_BELOW_FOLD_THRESHOLD,
            // Boolean which if true, will allow tooltips to show.
            ConfigKey.CARD_MENU_TOOLTIP_ELIGIBLE,
            // Boolean which causes synthetic tokens to be consumed when they are found.
            ConfigKey.CONSUME_SYNTHETIC_TOKENS,
            // Boolean which causes synthetic tokens to be automatically consume when restoring.
            // This will
            // not cause synthetic tokens to be consumed when opening with a new session.
            ConfigKey.CONSUME_SYNTHETIC_TOKENS_WHILE_RESTORING,
            ConfigKey.DEFAULT_ACTION_TTL_SECONDS,
            // When enabled, will ask server to add carousels in response.
            ConfigKey.ENABLE_CAROUSELS,
            // Time in ms to wait for image loading before showing a fade in animation.
            ConfigKey.FADE_IMAGE_THRESHOLD_MS,
            // Maximum number of times we will try to upload an action before deleting it.
            ConfigKey.FEED_ACTION_MAX_UPLOAD_ATTEMPTS,
            // The endpoint used for recording uploaded actions to the server.
            ConfigKey.FEED_ACTION_SERVER_ENDPOINT,
            // Maximum number of actions to be uploaded to the enpoint in a single request.
            ConfigKey.FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST,
            // Maximum size of the request to be uploaded to the enpoint in a single request.
            ConfigKey.FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST,
            // The method call to the feed action server  (put/post/etc)
            ConfigKey.FEED_ACTION_SERVER_METHOD,
            // If the feedActionServer response is length prefixed
            ConfigKey.FEED_ACTION_SERVER_RESPONSE_LENGTH_PREFIXED,
            // Ttl of an action that has failed to be uploaded.
            ConfigKey.FEED_ACTION_TTL_SECONDS,
            ConfigKey.FEED_SERVER_ENDPOINT,
            ConfigKey.FEED_SERVER_METHOD,
            ConfigKey.FEED_SERVER_RESPONSE_LENGTH_PREFIXED,
            // Boolean which if true, will ask the server for the feed ui response.
            ConfigKey.FEED_UI_ENABLED,
            ConfigKey.INITIAL_NON_CACHED_PAGE_SIZE,
            // Only update HEAD and the session making a page request
            ConfigKey.LIMIT_PAGE_UPDATES,
            // Do not update HEAD when making a page request
            ConfigKey.LIMIT_PAGE_UPDATES_IN_HEAD,
            // Time in ms that content should be rendered in order to be considered an immediate
            // open
            ConfigKey.LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS,
            // Boolean which if true, will ask the server for an menu option to launch the interest
            // management customize page.
            ConfigKey.MANAGE_INTERESTS_ENABLED,
            // The maximum number of times that the GC task can re-enqueue itself.
            ConfigKey.MAXIMUM_GC_ATTEMPTS,
            ConfigKey.MINIMUM_VALID_ACTION_RATIO,
            ConfigKey.NON_CACHED_MIN_PAGE_SIZE,
            ConfigKey.NON_CACHED_PAGE_SIZE,
            ConfigKey.SESSION_LIFETIME_MS,
            // Boolean which if true, will ask the server for a snippet from the article.
            ConfigKey.SNIPPETS_ENABLED,
            // Delay before a spinner should be shown after content is requested. Only used for feed
            // wide
            // spinners, not for more button spinners.
            ConfigKey.SPINNER_DELAY_MS,
            // Minimum time before a spinner should show before disappearing. Only used for feed
            // wide
            // spinners, not for more button spinners.
            ConfigKey.SPINNER_MINIMUM_SHOW_TIME_MS,
            // The number of items that can be missing from a call to FeedStore before failing.
            ConfigKey.STORAGE_MISS_THRESHOLD,
            // Time in ms for the length of the timeout
            ConfigKey.TIMEOUT_TIMEOUT_MS,
            ConfigKey.TRIGGER_IMMEDIATE_PAGINATION,
            // Turn on the Timeout Scheduler
            // Boolean which if true, will ask the server for not interested in actions and a
            // durable
            // dismiss action.
            ConfigKey.UNDOABLE_ACTIONS_ENABLED,
            // Use direct storage
            ConfigKey.USE_DIRECT_STORAGE,
            // Boolean which if true, will ask the server for a different pagination strategy.
            ConfigKey.USE_SECONDARY_PAGE_REQUEST,
            ConfigKey.USE_TIMEOUT_SCHEDULER,
            // Percentage of the view that should be on screen to log a view
            ConfigKey.VIEW_LOG_THRESHOLD,
            // Time in ms for the minimum amount of time a view must be onscreen to count as a view.
            ConfigKey.VIEW_MIN_TIME_MS,
    })
    public @interface ConfigKey {
        // Keep sorted.
        String ABANDON_RESTORE_BELOW_FOLD = "abandon_restore_below_fold";
        String ABANDON_RESTORE_BELOW_FOLD_THRESHOLD = "abandon_restore_below_fold_threshold";
        String CARD_MENU_TOOLTIP_ELIGIBLE = "card_menu_tooltip_eligible";
        String CONSUME_SYNTHETIC_TOKENS = "consume_synthetic_tokens_bool";
        String CONSUME_SYNTHETIC_TOKENS_WHILE_RESTORING =
                "consume_synthetic_tokens_while_restoring_bool";
        String DEFAULT_ACTION_TTL_SECONDS = "default_action_ttl_seconds";
        String ENABLE_CAROUSELS = "enable_carousels";
        String FADE_IMAGE_THRESHOLD_MS = "fade_image_threshold_ms";
        String FEED_ACTION_MAX_UPLOAD_ATTEMPTS = "feed_action_max_upload_attempts";
        String FEED_ACTION_SERVER_ENDPOINT = "feed_action_server_endpoint";
        String FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST =
                "feed_action_server_max_actions_per_request";
        String FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST = "feed_action_server_max_size_per_request";
        String FEED_ACTION_SERVER_METHOD = "feed_action_server_method";
        String FEED_ACTION_SERVER_RESPONSE_LENGTH_PREFIXED =
                "feed_action_server_response_length_prefixed";
        String FEED_ACTION_TTL_SECONDS = "feed_action_ttl_seconds";
        String FEED_SERVER_ENDPOINT = "feed_server_endpoint";
        String FEED_SERVER_METHOD = "feed_server_method";
        String FEED_SERVER_RESPONSE_LENGTH_PREFIXED = "feed_server_response_length_prefixed";
        String FEED_UI_ENABLED = "feed_ui_enabled";
        String INITIAL_NON_CACHED_PAGE_SIZE = "initial_non_cached_page_size";
        String LIMIT_PAGE_UPDATES = "limit_page_updates";
        String LIMIT_PAGE_UPDATES_IN_HEAD = "limit_page_updates_in_head";
        String LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS = "logging_immediate_content_threshold_ms";
        String MANAGE_INTERESTS_ENABLED = "manage_interests_enabled";
        String MAXIMUM_GC_ATTEMPTS = "maximum_gc_attempts";
        String MINIMUM_VALID_ACTION_RATIO = "minimum_valid_action_ratio";
        String NON_CACHED_MIN_PAGE_SIZE = "non_cached_min_page_size";
        String NON_CACHED_PAGE_SIZE = "non_cached_page_size";
        String SESSION_LIFETIME_MS = "session_lifetime_ms";
        String SNIPPETS_ENABLED = "snippets_enabled";
        String SPINNER_DELAY_MS = "spinner_delay";
        String SPINNER_MINIMUM_SHOW_TIME_MS = "spinner_minimum_show_time";
        String STORAGE_MISS_THRESHOLD = "storage_miss_threshold";
        String TIMEOUT_TIMEOUT_MS = "timeout_timeout_ms";
        String TRIGGER_IMMEDIATE_PAGINATION = "trigger_immediate_pagination_bool";
        String UNDOABLE_ACTIONS_ENABLED = "undoable_actions_enabled";
        String USE_DIRECT_STORAGE = "use_direct_storage";
        String USE_SECONDARY_PAGE_REQUEST = "use_secondary_page_request";
        String USE_TIMEOUT_SCHEDULER = "use_timeout_scheduler";
        String VIEW_LOG_THRESHOLD = "view_log_threshold";
        String VIEW_MIN_TIME_MS = "view_min_time_ms";
    }

    private final HashMap<String, Object> mValues;

    private Configuration(HashMap<String, Object> values) {
        this.mValues = values;
    }

    public String getValueOrDefault(String key, String defaultValue) {
        return this.<String>getValueOrDefaultUnchecked(key, defaultValue);
    }

    public long getValueOrDefault(String key, long defaultValue) {
        return this.<Long>getValueOrDefaultUnchecked(key, defaultValue);
    }

    public boolean getValueOrDefault(String key, boolean defaultValue) {
        return this.<Boolean>getValueOrDefaultUnchecked(key, defaultValue);
    }

    public double getValueOrDefault(String key, double defaultValue) {
        return this.<Double>getValueOrDefaultUnchecked(key, defaultValue);
    }

    /**
     * Returns the value if it exists, or {@code defaultValue} otherwise.
     *
     * @throws ClassCastException if the value can't be cast to {@code T}.
     */
    private <T> T getValueOrDefaultUnchecked(String key, T defaultValue) {
        if (mValues.containsKey(key)) {
            // The caller assumes the responsibility of ensuring this cast succeeds
            @SuppressWarnings("unchecked")
            T castedValue = (T) mValues.get(key);
            return castedValue;
        } else {
            return defaultValue;
        }
    }

    /** Returns true if a value exists for the {@code key}. */
    public boolean hasValue(String key) {
        return mValues.containsKey(key);
    }

    /** Returns a {@link Builder} for this {@link Configuration}. */
    public Builder toBuilder() {
        return new Builder(mValues);
    }

    /** Returns a default {@link Configuration}. */
    public static Configuration getDefaultInstance() {
        return new Builder().build();
    }

    /** Builder class used to create {@link Configuration} objects. */
    public static final class Builder {
        private final HashMap<String, Object> mValues;

        private Builder(HashMap<String, Object> values) {
            this.mValues = new HashMap<>(values);
        }

        public Builder() {
            this(new HashMap<>());
        }

        public Builder put(@ConfigKey String key, String value) {
            mValues.put(key, value);
            return this;
        }

        public Builder put(@ConfigKey String key, long value) {
            mValues.put(key, value);
            return this;
        }

        public Builder put(@ConfigKey String key, boolean value) {
            mValues.put(key, value);
            return this;
        }

        public Builder put(@ConfigKey String key, double value) {
            mValues.put(key, value);
            return this;
        }

        public Configuration build() {
            return new Configuration(mValues);
        }
    }
}
