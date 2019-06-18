class FakeSerialService {
  constructor() {
    this.interceptor_ =
        new MojoInterfaceInterceptor(blink.mojom.SerialService.name);
    this.interceptor_.oninterfacerequest = e => this.bind(e.handle);
    this.bindingSet_ = new mojo.BindingSet(blink.mojom.SerialService);
    this.nextToken_ = 0;
    this.reset();
  }

  start() {
    this.interceptor_.start();
  }

  stop() {
    this.interceptor_.stop();
  }

  reset() {
    this.ports_ = new Map();
    this.selectedPort_ = null;
  }

  addPort(vendorId, productId) {
    let info = new blink.mojom.SerialPortInfo();
    if (vendorId !== undefined) {
      info.hasVendorId = true;
      info.vendorId = vendorId;
    }
    if (productId !== undefined) {
      info.hasProductId = true;
      info.productId = productId;
    }
    let token = ++this.nextToken_;
    info.token = new mojoBase.mojom.UnguessableToken();
    info.token.high = 0;
    info.token.low = token;
    this.ports_.set(token, info);
    return token;
  }

  removePort(token) {
    this.ports_.delete(token);
  }

  setSelectedPort(token) {
    this.selectedPort_ = this.ports_.get(token);
  }

  bind(handle) {
    this.bindingSet_.addBinding(this, handle);
  }

  async getPorts() {
    return { ports: Array.from(this.ports_.values()) };
  }

  async requestPort(filters) {
    return { port: this.selectedPort_ };
  }
}

let fakeSerialService = new FakeSerialService();

function serial_test(func, name, properties) {
  promise_test(async (test) => {
    fakeSerialService.start();
    try {
      await func(test, fakeSerialService);
    } finally {
      fakeSerialService.stop();
      fakeSerialService.reset();
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
