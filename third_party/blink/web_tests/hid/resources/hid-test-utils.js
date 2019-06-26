// A fake implementation of the HidService mojo interface. HidService manages
// HID device access for clients in the render process. Typically, when a client
// requests access to a HID device a chooser dialog is shown with a list of
// available HID devices. Selecting a device from the chooser also grants
// permission for the client to access that device.
//
// The fake implementation allows tests to simulate connected devices. It also
// skips the chooser dialog and instead allows tests to specify which device
// should be selected. All devices are treated as if the user had already
// granted permission.
class FakeHidService {
  constructor() {
    this.interceptor_ =
        new MojoInterfaceInterceptor(blink.mojom.HidService.name);
    this.interceptor_.oninterfacerequest = e => this.bind(e.handle);
    this.bindingSet_ = new mojo.BindingSet(blink.mojom.HidService);
    this.nextGuidValue_ = 0;
    this.reset();
  }

  start() {
    this.interceptor_.start();
  }

  stop() {
    this.interceptor_.stop();
  }

  reset() {
    this.devices_ = new Map();
    this.selectedDevice_ = null;
  }

  // Creates and returns a HidDeviceInfo with the specified device IDs.
  makeDevice(vendorId, productId) {
    let guidValue = ++this.nextGuidValue_;
    let info = new device.mojom.HidDeviceInfo();
    info.guid = guidValue.toString();
    info.vendorId = vendorId;
    info.productId = productId;
    info.productName = 'product name';
    info.serialNumber = '0';
    info.reportDescriptor = new Uint8Array();
    info.collections = [];
    info.deviceNode = 'device node';
    return info;
  }

  // Simulates a connected device. Returns the device GUID. A device connection
  // event is not generated.
  addDevice(deviceInfo) {
    this.devices_.set(deviceInfo.guid, deviceInfo);
    return deviceInfo.guid;
  }

  // Simulates disconnecting a connected device. A device disconnection event is
  // not generated.
  removeDevice(guid) {
    this.devices_.delete(guid);
  }

  // Sets the GUID of the device that will be returned as the selected item the
  // next time requestDevice is called. The device with this GUID must have been
  // previously added with addDevice.
  setSelectedDevice(guid) {
    this.selectedDevice_ = this.devices_.get(guid);
  }

  bind(handle) {
    this.bindingSet_.addBinding(this, handle);
  }

  // Returns an array of connected devices. Normally this would only include
  // devices that the client has already been granted permission to access, but
  // for the fake implementation all simulated devices are returned.
  async getDevices() {
    return { devices: Array.from(this.devices_.values()) };
  }

  // Simulates a device chooser prompt, returning |selectedDevice_| as the
  // simulated selection. |filters| is ignored.
  async requestDevice(filters) {
    return { device: this.selectedDevice_ };
  }
}

let fakeHidService = new FakeHidService();

function hid_test(func, name, properties) {
  promise_test(async (test) => {
    fakeHidService.start();
    try {
      await func(test, fakeHidService);
    } finally {
      fakeHidService.stop();
      fakeHidService.reset();
    }
  }, name, properties);
}

function trustedClick() {
  return new Promise(resolve => {
    let button = document.createElement('button');
    button.textContent = 'click to continue test';
    button.style.display = 'block';
    button.style.fontSize = '20px';
    button.style.padding = '10px';
    button.onclick = () => {
      document.body.removeChild(button);
      resolve();
    };
    document.body.appendChild(button);
    test_driver.click(button);
  });
}
