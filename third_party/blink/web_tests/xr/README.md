# XR

This directory contains tests that cannot be external/wpt/webxr tests due to one
one of the following reasons:

1) The behavior they test is chrome-specific in some way
(e.g. intentionally non-compliant, specific decisions allowed by the spec, non-specced interactions, etc.)

2) They test behavior and interactions with or exposed by the mojom layer that are not suitable to be shared.

3) They rely on test methods that are not present in the webxr-test-api

4) They test behavior that is not yet in the core spec.

In the case of number 4 a crbug should be filed to update and move the test to be external once the spec has been updated.

## XR/Resources
The resources folder contains the xr device mocking extensions that enable some internal tests.
Due to pathing limitations, the xr_session_promise_test implementation is also cloned into xr-test-utils

Where possible, ensure that any updates to xr-test-utils can be mirrored into external/wpt/webxr/webxr_utils.
Any helper methods should also generally be added to the external resources, to ease the transition of temporarily-internal tests to be external.

