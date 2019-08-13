# Flag Expiry

This document outlines the process by which flags in chromium expire and are
removed from the codebase, and describes which flags are about to expire. This
is the authoritative list of flags that are expiring and being removed. This
document only describes entries in chrome://flags, *not* command-line switches
(commonly also called command-line flags). This process does not cover
command-line switches and there continue to be no guarantees about those.

[TOC]

## Do Not Depend On Flags

If you are using (or think you need to use) a flag to configure Chromium for
your use case, please [file a bug] or email flags-dev@, because that flag will
likely be removed at some point.

Flags have never been a supported configuration surface in Chromium, and we have
never guaranteed that any specific flag will behave consistently or even
continue to exist. This document describes a process for removing flags that
have been around for long enough that users *might* have come to rely on their
ongoing existence in a way that hopefully minimizes pain, but Chromium
developers are free to change the behavior of or remove flags at any time. In
particular, just because a flag will expire through this process does not mean a
developer will not remove it earlier than this process specifies.

## The Process

After each milestone's branch point:

1. The flags team chooses a set of flags to begin expiring, from the list
   produced by `tools/flags/list-flags.py --expired-by $MSTONE`. In the steady
   state, when there is not a big backlog of flags to remove, this set will be
   the entire list of flags that are `expired-by $MSTONE`.
2. The flags team hides the flags in this set by default from `chrome://flags`,
   and adds a flag `temporary-unexpire-flags-M$MSTONE` and a base::Feature
   `TemporaryUnexpireFlagsM$MSTONE` which unhide these flags. When hidden from
   `chrome://flags`, all the expired flags will behave as if unset, so users
   cannot be stuck with a non-default setting of a hidden flag.
3. After two further milestones have passed (i.e. at $MSTONE+2 branch), the
   temporary unhide flag & feature will be removed (meaning the flags are now
   permanently invisible), and TPMs will file bugs against the listed owners to
   remove the flags and clean up the backing code.

There are other elements of this process not described here, such as emails to
flags-dev@ tracking the status of the process.

## The Set

In M77, the following flags are being hidden as the second step of this process.
If you are using one of these flags for some reason, please get in touch with
the flags team (via flags-dev@) and/or the listed owner(s) of that flag. This
list will be updated at each milestone as we expire more flags. This is the
authoritative source of the expiry set for a given milestone.

* allow-remote-context-for-notifications
* android-files-in-files-app
* app-service-ash
* arc-available-for-child
* arc-boot-completed-broadcast
* arc-custom-tabs-experiment
* arc-documents-provider
* arc-graphics-buffer-visualization-tool
* arc-native-bridge-experiment
* arc-usb-host
* arc-vpn
* ash-enable-pip-rounded-corners
* autofill-always-show-server-cards-in-sync-transport
* autofill-enable-company-name
* autofill-enable-local-card-migration-for-non-sync-user
* autofill-enforce-min-required-fields-for-heuristics
* autofill-enforce-min-required-fields-for-query
* autofill-enforce-min-required-fields-for-upload
* autofill-restrict-formless-form-extraction
* autofill-rich-metadata-queries
* autofill-settings-split-by-card-type
* background-task-component-update
* calculate-native-win-occlusion
* cct-module-cache
* cct-module-custom-request-header
* cct-module-dex-loading
* cct-module-post-message
* cct-module-use-intent-extras
* crostini-usb-support
* document-passive-wheel-event-listeners
* enable-app-list-search-autocomplete
* enable-arc-cups-api
* enable-arc-unified-audio-focus
* enable-autofill-credit-card-upload-editable-cardholder-name
* enable-autofill-credit-card-upload-editable-expiration-date
* enable-autofill-do-not-upload-save-unsupported-cards
* enable-autofill-import-dynamic-forms
* enable-autofill-import-non-focusable-credit-card-forms
* enable-autofill-local-card-migration-uses-strike-system-v2
* enable-autofill-send-experiment-ids-in-payments-rpcs
* enable-bulk-printers
* enable-chromeos-account-manager
* enable-custom-mac-paper-sizes
* enable-encryption-migration
* enable-experimental-accessibility-features
* enable-experimental-accessibility-language-detection
* enable-fs-nosymfollow
* enable-google-branded-context-menu
* enable-immersive-fullscreen-toolbar
* enable-myfiles-volume
* enable-native-controls
* enable-native-google-assistant
* enable-reopen-tab-in-product-help
* enable-safe-browsing-ap-download-verdicts
* enable-webrtc-hw-vp9-encoding
* enable-webrtc-pipewire-capturer
* enable-zero-state-suggestions
* enable_messages_web_push
* force-use-chrome-camera
* foreground-notification-manager
* gdi-text-printing
* handwriting-gesture
* manual-password-generation-android
* offline-indicator-always-http-probe
* offline-pages-ct-suppress-completed-notification
* offline-pages-load-signal-collecting
* offline-pages-resource-based-snapshot
* omnibox-experimental-keyword-mode
* on-the-fly-mhtml-hash-computation
* pdf-annotations
* postscript-printing
* rewrite-leveldb-on-deletion
* session-restore-prioritizes-background-use-cases
* smart-text-selection
* stop-in-background
* ui-show-composited-layer-borders
* unfiltered-bluetooth-devices
* unsafely-treat-insecure-origin-as-secure
* use_messages_google_com_domain
* use_messages_staging_url
* wake-on-wifi-packet

[file a bug]: https://new.crbug.com
