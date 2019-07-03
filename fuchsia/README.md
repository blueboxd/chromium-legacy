# Chromium-based Fuchsia services
This directory contains implementation code for various Fuchsia services living
in the Chromium repository. To build Chromium on Fuchsia, check this
[documentation](../docs/fuchsia_build_instructions.md).

[TOC]

## Code organization
Each of the following subdirectories contain code for a specific Fuchsia
service:
* `./engine` contains the WebEngine implementation. The WebEngine enables
Fuchsia applications to embed Chromium frames for rendering web content.
* `./http` contains an implementation for the Fuchsia HTTP service.
* `./runners`contains implementations of Fuchsia `sys.runner`.
    * `./runners/cast` Enables the Fuchsia system to launch cast applications.
    * `./runners/web` Enables the Fuchsia system to launch HTTP or HTTPS URLs.

When writing a new Fuchsia service, it is recommended to create a new
subdirectory under `//fuchsia` or a new subdirectory under `//fuchsia/runners`
depending on the use case.

The `./base` subdirectory contains common utilities used by more than one of
the aforementioned Fuchsia services.

The `./cipd` and `./fidl` subdirectories contain CIPD definitions and FIDL
interface definitions, respectfully.

### Namespacing

Code that is not shared across multiple targets should live in the global
namespace. Code that is shared across multiple targets should live in the
`cr_fuchsia` namespace.

### Test code

Under the `//fuchsia` directory , there are 3 major types of tests:
* Unit tests: Exercises a single class in isolation, allowing full control
  over the external environment of this class.
* Browser tests: Spawns a full browser process along child processes. The test
  code is run inside the browser process, allowing for full access to the
  browser code, but not other processes.
* Integration tests: they exercise the published API of a Fuchsia component. For
  instance, `//fuchsia/engine:web_engine_integration_tests` make use of the
  `//fuchsia/engine:web_engine` component. The test code is run in a separate
  process in a separate component, allowing only access to the published API of
  the component under test.

Integration tests are more resource-intensive than browser tests, which are in
turn more expensive than unit tests. Therefore, when writing new tests, it is
preferred to write unit tests over browser tests over integration tests.

As a general rule, test-only code should live in the same directory as the code
under test with an explicit file name, either `fake_*`, `test_*`,
`*_unittest.cc`, `*_ browser_test.cc` or `*_integration_test.cc`.

Test code that is shared across components should live in a dedicated `test`
directory, under the `cr_fuchsia` namespace. For instance, see the
`//fuchsia/engine/test` directory, which contains code shared by all browser
tests.

## Building and deploying the WebRunner service

When you build `web_runner`, Chromium will automatically generate scripts for
you that will automatically provision a device with Fuchsia and then install
`web_runner` and its dependencies.

To build and run `web_runner`, follow these steps:

1. (Optional) Ensure that you have a device ready to boot into Fuchsia.

    If you wish to have `web_runner` manage the OS deployment process, then you
    should have the device booting into
    [Zedboot](https://fuchsia.googlesource.com/zircon/+/master/docs/targets/usb_setup.md).

2. Build `web_runner`.

    ```bash
    $ autoninja -C out/fuchsia web_runner
    ```

3. Install `web_runner`.

    * For devices running Zedboot:

        ```bash
        $ out/fuchsia/bin/install_web_runner -d
        ```

    * For devices already booted into Fuchsia:

        You will need to add command line flags specifying the device's IP
        address and the path to the `ssh_config` used by the device
        (located at `$FUCHSIA_OUT_DIR/ssh-keys/ssh_config`):

        ```bash
        $ out/fuchsia/bin/install_web_runner -d --ssh-config $PATH_TO_SSH_CONFIG
        ```

4. Press Alt-Esc key on your device to switch back to terminal mode or run
`fx shell` from the host.

5. Launch a webpage.

    ```bash
    $ tiles_ctl add https://www.chromium.org/
    ```

6. Press Alt-Esc to switch back to graphical view if needed. The browser
window should be displayed and ready to use.

7. You can deploy and run new versions of Chromium without needing to reboot.

    First kill any running processes:

    ```bash
    $ killall chromium.cmx; killall web_runner.cmx
    ```

    Then repeat steps 1 through 6 from the installation instructions, excluding
    step #3 (running Tiles).


### Closing a webpage

1. Press the Windows key to return to the terminal.

2. Instruct tiles_ctl to remove the webpage's window tile. The tile's number is
    reported by step 6, or it can be found by running `tiles_ctl list` and
    noting the ID of the "url" entry.

    ```bash
    $ tiles_ctl remove TILE_NUMBER
    ```

## Debugging

Rudimentary debugging is now possible with zxdb which is included in the SDK.
It is still early and fairly manual to set up. After following the steps above:

1. To get your device IP address, run `ifconfig` on the device or `fx netaddr`
on the host.

2. On the device, run `run debug_agent --port=2345`.

3. On the host, run

    ```bash
    third_party/fuchsia_sdk/sdk/tools/zxdb -s out/fuchsia/exe.unstripped -s out/fuchsia/lib.unstripped
    ```

4. In zxdb, `connect <ip-from-sysinfo-above> 2345`.

5. On the device, run `ps` and find the pid of the process you want to debug,
e.g. `web_runner`.

6. In zxdb, `attach <pid>`. You should be able to attach to multiple processes.

7. In zxdb, `b ComponentControllerImpl::CreateForRequest` to set a breakpoint.

8. On device, do something to make your breakpoint be hit. In this case
`tiles_ctl add https://www.google.com/` should cause a new request.

At this point, you should hit the breakpoint in zxdb.

```
[zxdb] l
   25     fuchsia::sys::Package package,
   26     fuchsia::sys::StartupInfo startup_info,
   27     fidl::InterfaceRequest<fuchsia::sys::ComponentController>
   28         controller_request) {
   29   std::unique_ptr<ComponentControllerImpl> result{
 ▶ 30       new ComponentControllerImpl(runner)};
   31   if (!result->BindToRequest(std::move(package), std::move(startup_info),
   32                              std::move(controller_request))) {
   33     return nullptr;
   34   }
   35   return result;
   36 }
   37
   38 ComponentControllerImpl::ComponentControllerImpl(WebContentRunner* runner)
   39     : runner_(runner), controller_binding_(this) {
   40   DCHECK(runner);
[zxdb] f
▶ 0 webrunner::ComponentControllerImpl::CreateForRequest() • component_controller_impl.cc:30
  1 webrunner::WebContentRunner::StartComponent() • web_content_runner.cc:34
  2 fuchsia::sys::Runner_Stub::Dispatch_() • fidl.cc:1255
  3 fidl::internal::StubController::OnMessage() • stub_controller.cc:38
  4 fidl::internal::MessageReader::ReadAndDispatchMessage() • message_reader.cc:213
  5 fidl::internal::MessageReader::OnHandleReady() • message_reader.cc:179
  6 fidl::internal::MessageReader::CallHandler() • message_reader.cc:166
  7 base::AsyncDispatcher::DispatchOrWaitUntil() • async_dispatcher.cc:183
  8 base::MessagePumpFuchsia::HandleEvents() • message_pump_fuchsia.cc:236
  9 base::MessagePumpFuchsia::Run() • message_pump_fuchsia.cc:282
  10 base::MessageLoop::Run() + 0x22b (no line info)
  11 base::RunLoop::Run() • run_loop.cc:102
  12 main() • main.cc:74
  13 0x472010320b8f
  14 0x0
[zxdb]
```

This
[documentation](https://fuchsia.googlesource.com/garnet/+/master/docs/debugger.md#diagnosing-symbol-problems)
maybe be a useful reference if you do not see symbols. That page also has
general help on using the debugger.
