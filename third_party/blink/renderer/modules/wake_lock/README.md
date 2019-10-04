# modules/wake_lock

[TOC]

This directory contains an implementation of the [Wake Lock specification], a Web API that allows script authors to prevent both the screen from turning off ("screen wake locks") as well as the CPU from entering a deep power state ("system wake locks"). There are platform implementations for ChromeOS, Linux (X11), Mac, Android, and Windows.

At the time of writing (August 2019), system wake lock requests are always denied, as allowing them depends on a proper permission model for the requests being figured out first.

The code required to implement the Wake Lock API is spread across multiple Chromium subsystems: Blink, `//content`, `//services` and `//chrome`. This document focuses on the Blink part, and the other subsystems are mentioned when necessary but without much detail.

## High level overview

Wake Lock's API surface is fairly small; the `WakeLock` IDL interface only has static methods, and there is no state that can be easily inspected. Additionally, all the parts that actually communicate with platform APIs are implemented elsewhere, so the Blink side only exposes the JavaScript API to script authors, validates API calls and manages [state records].

The three main Blink classes implementing the spec are:

* [`WakeLock`](wake_lock.h): implements the API bindings following the spec. Like its IDL counterpart, it only has static methods. Permission request and lock acquisition calls are all forwarded to `WakeLockController`.
* [`WakeLockController`](wake_lock_controller.h): per-`ExecutionContext` class. It implements all permission management required by the spec, as well as the [Wake Lock management] tasks that apply to documents and/or workers (e.g. page visibility handling). Its `state_records_` array contains per-wake lock type `WakeLockStateRecord` instances, so all wake lock types are managed independently.
* [`WakeLockStateRecord`](wake_lock_state_record.h): Owned by `WakeLockController`. This is an implementation of the [state records] Wake Lock concept in a per-type fashion. Like in the spec, it keeps track of all active locks of a certain type, and it is also responsible for communicating with the `//content` and `//services` layers to request and cancel wake locks.

Furthermore, [`wake_lock.mojom`](../../../public/mojom/wake_lock/wake_lock.mojom) defines the Mojo interface implemented by `//content`'s [`WakeLockServiceImpl`](/content/browser/wake_lock/wake_lock_service_impl.h) that `WakeLockStateRecord` uses to obtain a [`device::mojom::blink::WakeLock`](/services/device/public/mojom/wake_lock.mojom) and request/cancel a wake lock.

The rest of the implementation is found in the following directories:

* `content/browser/wake_lock` [implements](/content/browser/wake_lock/wake_lock_service_impl.cc) the [`WakeLockService`](../../../public/mojom/wake_lock/wake_lock.mojom) Mojo interface defined in Blink. It is responsible for communicating with Blink and connecting Blink to `services/device/wake_lock`.
* `services/device/wake_lock` contains the [platform-specific parts of the implementation](../../../../../services/device/wake_lock/power_save_blocker) and implements the Wake Lock [Mojo interfaces].
* `chrome/browser/wake_lock` contains the Chrome-specific side of permission management for Wake Locks. When the Blink implementation needs to either query or request permission for wake locks, the request bubbles up to this directory, where the decision is made based on the wake lock type (for testing purposes, `content_shell` always grants screen wake locks and denies system wake locks in [`shell_permission_manager.cc`](/content/shell/browser/web_test/web_test_message_filter.cc)).

[Mojo interfaces]: ../../../../../services/device/public/mojom/
[Wake Lock management]: https://w3c.github.io/wake-lock/#managing-wake-locks
[Wake Lock specification]: https://w3c.github.io/wake-lock/
[state records]: https://w3c.github.io/wake-lock/#dfn-state-record

### Testing

Validation, exception types, feature policy integration and general IDL compliance are tested in [web platform tests], while Chromium-specific implementation details (e.g. permission handling) are tested in [web tests].

Larger parts of the Blink implementation are tested as browser and unit tests:

* `*_test.cc` and `wake_lock_test_utils.{cc,h}` are built as part of the `blink_unittests` GN target, and attempt to have coverage over most of the code in this directory.
* The unit tests in `services/device/wake_lock` test the service side of the API implementation.
* `chrome/browser/wake_lock` has unit tests for `WakeLockPermissionContext`, and browser tests for end-to-end behavior testing.
* content_shell implements its own permission logic that mimics what is done in `//chrome` in [`shell_permission_manager.cc`](/content/shell/browser/shell_permission_manager.cc).

[web platform tests]: ../../../web_tests/external/wpt/wake-lock/
[web tests]: ../../../web_tests/wake-lock/

## Example workflows

### Wake Lock acquisition

This section describes how the classes described above interact when the following excerpt is run in the browser:

``` javascript
const abortController = new AbortController();
const lockPromise = WakeLock.request("screen", { signal: abortController.signal });
```

1. `WakeLock::request()` performs all the validation steps described in [the spec](https://w3c.github.io/wake-lock/#request-static-method).
1. Since an [AbortSignal] was passed in the JavaScript call, `WakeLock::request()` will call [`AbortSignal::AddAlgorithm()`](../../core/dom/abort_signal.h) and add `WakeLockController::ReleaseWakeLock()` to its list of algorithms [n.b. see below the section below for what happens if `abortController.abort()` is called].
1. If all checks have passed, it calls `WakeLockController::From()` to either create or obtain a `WakeLockController` attached to the given execution context (i.e. a `Document` or `DedicatedWorkerGlobalScope`).
1. `WakeLockController::RequestWakeLock()` is called, and `WakeLock::request()` then returns a promise. `RequestWakeLock()` is a small method that just verifies some invariants and calls `WakeLockController::ObtainPermission()`.
1. `WakeLockController::ObtainPermission()` connects to the [permission service](../../../public/mojom/permissions/permission.mojom) and asynchronously requests permission for a screen wake lock.
1. In the browser process, the permission request bubbles up through `//content` and reaches `//chrome`'s [`WakeLockPermissionContext`](/chrome/browser/wake_lock/wake_lock_permission_context.cc), where `WakeLockPermissionContext::GetPermissionStatusInternal()` always grants `CONTENT_SETTINGS_TYPE_WAKE_LOCK_SCREEN` permission requests.
1. Back in Blink, the permission request callback in this case is `WakeLockController::DidReceivePermissionResponse()`. It performs some sanity checks such as verifying the wake lock's corresponding [AbortSignal] has not been triggered. If any of the checks fail, the `ScriptPromiseResolver` instance created earlier by `WakeLock::request()` is rejected and we stop here. If everything went well, calls `WakeLockController::AcquireWakeLock()`.
1. `WakeLockController::AcquireWakeLock()` is a wrapper that invokes `WakeLockStateRecord::AcquireWakeLock()`.
1. If there are no existing screen wake locks, `WakeLockStateRecord::AcquireWakeLock()` will connect to the `WakeLockService` Mojo interface, invoke its `GetWakeLock()` method to obtain a `device::mojom::blink::WakeLock` and call its `RequestWakeLock()` method.
1. Otherwise, if there is at least one existing screen lock, `WakeLockStateRecord::AcquireWakeLock()` will simply add the `ScriptPromiseResolver` to its set of [active locks].

[AbortSignal]: https://dom.spec.whatwg.org/#interface-AbortSignal
[active locks]: https://w3c.github.io/wake-lock/#dfn-activelocks

### Wake Lock cancellation

Given the excerpt below:

``` javascript
const abortController = new AbortController();
const lockPromise = WakeLock.request("screen", { signal: abortController.signal });
abortController.abort();
```

This section describes what happens when `abortController.abort()` is called.

1. `abortController.abort()` causes `AbortSignal` to go through its list of algorithms, and `WakeLockController::ReleaseWakeLock()` is eventually called.
1. `WakeLockController::ReleaseWakeLock()` itself is a small function that simply calls `WakeLockStateRecord::ReleaseWakeLock()`.
1. `WakeLockStateRecord::ReleaseWakeLock()` implements the spec's [release wake lock algorithm]. One interesting aspect is that it rejects any `ScriptPromiseResolver` passed to it, even if it is not in its active locks list. This is caused by the fact that `WakeLock.request()` contains parallel steps, and script authors have access to the promise it returns before a lock has actually been requested and added to `WakeLockStateRecord`'s active locks list.
1. If the given wake lock is in `WakeLockStateRecord`'s `active_locks_`, it will be removed and, if the list is empty, `WakeLockStateRecord` will communicate with its `device::mojom::blink::WakeLock` instance and call its `CancelWakeLock()` method.

[release wake lock algorithm]: https://w3c.github.io/wake-lock/#release-wake-lock-algorithm

## Other Wake Lock usage in Chromium

### Inside Blink

Video playback via the `<video>` tag currently uses a screen wake lock behind the scenes to prevent the screen from turning off while a video is being played. The implementation can be found in the [`VideoWakeLock`](../../core/html/media/video_wake_lock.h) class.

This is an implementation detail, but the code handling wake locks in `VideoWakeLock` is similar to `WakeLockStateRecord`'s, where Blink needs to talk to `//content` to connect to a `WakeLockServiceImpl` and use that to get to a `device::mojom::blink::WakeLock`.

**Note:** when writing new code that uses Wake Locks in Blink, it is recommended to follow the same pattern outlined above. That is, connect to `WakeLockService` via the `//content` layer and request a `device::mojom::blink::WakeLock` via `WakeLockService::GetWakeLock()`. Do not go through the classes in this module, and [**do not connect to the Wake Lock services directly**](/docs/servicification.md#Frame-Scoped-Connections). See the example below:

```c++
mojo::Remote<mojom::blink::WakeLockService> wake_lock_service;
mojo::Remote<device::mojom::blink::WakeLock> wake_lock;
execution_context->GetInterface(wake_lock_service.BindNewPipeAndPassReceiver());
wake_lock_service->GetWakeLock(..., wake_lock.BindNewPipeAndPassReceiver());
wake_lock_->RequestWakeLock();
```

### Outside Blink

The [Wake Lock service](/services/device/wake_lock) is also used by multiple parts of Chromium outside Blink. In other words, it is possible to use the Wake Lock service and prevent screen and CPU from entering a deep power state directly from the browser side.

In fact, this is why [`WakeLockProvider::GetWakeLockWithoutContext()`](/services/device/public/mojom/wake_lock_provider.mojom) exists in the first place. One consequence is that one needs to bear in mind these other usages when changing the public API exposed by the Wake Lock service.

**Note:** Avoid using `WakeLockProvider::GetWakeLockWithoutContext()` whenever possible. By design, it does not work on Android.

Example usage outside Blink includes:

* ChromeOS's [encryption migration screen handler](/chrome/browser/ui/webui/chromeos/login/encryption_migration_screen_handler.cc)
* Media capture code in [content/browser](/content/browser/media/capture/desktop_capture_device.cc)
* The Google Drive [component](/components/drive/drive_uploader.cc)
* The `chrome.power` [extension API](/extensions/browser/api/power/power_api.cc)

## Permission Model

The Wake Lock API spec checks for user activation in the context of [wake lock permission requests](https://w3c.github.io/wake-lock/#dfn-obtain-permission), either as a result of a call to either `WakeLock.requestPermission()` or `WakeLock.request()`. If a user agent is configured to prompt a user when a wake lock is requested, user activation is required, otherwise the request will be denied.

In the Chromium implementation, there currently is no "prompt" state, and no permission UI or settings: wake lock requests are either always granted or always denied:

* Screen wake lock request are always granted without prompting or user activation checks. This is based on the existing precedent of the `<video>` tag's use of `VideoWakeLock`s: they are always requested and granted transparently, so even if the Wake Lock API implementation in Chromium started requiring stricter checks, malicious actors could still embed a `<video>` tag and prevent the screen from turning off without any user interaction.

* System wake lock requests are always denied in `chrome/browser/wake_lock/wake_lock_permission_context.cc`. This means the entirety of the code is present and enabled in Blink, but all calls to `WakeLock.request('system')` currently return a promise that will be rejected with a `NotAllowedError`. Changing that requires figuring out a permission model for system wake lock requests, which, at the moment, is future work.
