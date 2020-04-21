# Running in Chrome

When a Trusted Web Activity (TWA) is launched to Chrome, it it shares Chrome's
Profile and storage partition.
This means that if you're logged into a website in Chrome, you're logged into
that website in the Trusted Web Activity.
To give the users an understanding of how their data is used and shared, we
must inform them of this.
(The behaviour described here is carried out independently for each TWA the
user installs and runs.)

## Status

This is a work is progress and this file will be updated as implementation is
carried out.
At present, the old behavior is on by default and the new behavior can be
enabled with the `"TrustedWebActivityNewDisclosure"` feature.

Of the new behavior, only the *If notifications are not enabled* part has
been implemented.
The corresponding bug is [here](https://crbug.com/1068106).

## Chrome before XX

From Chrome 72 (when TWAs were launched) to Chrome XX, we informed the users by
showing them a "Running in Chrome" Infobar.
This Infobar would be shown to the users the first time they launched the TWA
and would persist until they dismissed it.

## Chrome since XX

Developers found the previous behavior intrusive and annoying, reducing the
amount of the screen that shows the webpage.
The new behavior now depends on whether the user has notifications for Chrome
enabled.

### If Chrome's notifications are enabled

The first time a user opens a TWA in Chrome, they will get a silent, high
priority notification informing them that their data is shared with Chrome.
If the user taps on the notification or dismisses it, they won't trigger this
behavior again.
If the user does not interact with the notification, the next time they open a
TWA, they will get a similar notification, except it will now be low priority.

### If Chrome's notifications are not enabled

The first time a user opens a TWA in Chrome, they will see a Snackbar informing
them that their data is shared with Chrome.
If the user interacts with the Snackbar, it is gone for good, if they do not,
it returns the next time the TWA is launched.

A Snackbar is different from an Infobar (what we showed previously) in that a
Snackbar will dismiss itself after some time (in this case, 7 seconds) whereas
an Infobar will not.

## Code

The old behavior is managed by the `TrustedWebActivityDisclosureController`,
which communicates through the `TrustedWebActivityModel` to the
`TrustedWebActivityDisclosureView`.

When the new behavior is enabled, the `TrustedWebActivityDisclosureView` is
replaced by the `NewSnackbarDisclosure`.
