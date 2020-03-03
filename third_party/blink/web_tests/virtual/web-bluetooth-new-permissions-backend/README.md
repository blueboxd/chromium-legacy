# Web Bluetooth New Permissions Backend

This virtual test suite runs content_shell with
`--enable-features=WebBluetoothNewPermissionsBackend`. This flag enables the
Web Bluetooth tests to use the
[`FakeBluetoothDelegate`](https://source.chromium.org/chromium/chromium/src/+/master:content/shell/browser/web_test/fake_bluetooth_delegate.h)
interface for granting and checking permissions. This class emulates the
behavior of the new Web Bluetooth permissions backend based on
[`ChooserContextBase`](https://source.chromium.org/chromium/chromium/src/+/master:chrome/browser/permissions/chooser_context_base.h).

The new permissions backend is implemented as part of the [Web Bluetooth
Persistent Permissions project](https://docs.google.com/document/d/1h3uAVXJARHrNWaNACUPiQhLt7XI-fFFQoARSs1WgMDM).

TODO(https://crbug.com/589228): Remove this virtual test suite when the
`WebBluetoothNewPermissionsBackend` flag is enabled by default.