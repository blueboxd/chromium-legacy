'use strict';

class MockEyeDropperChooser {
  constructor() {
    this.bindingSet_ = new mojo.BindingSet(blink.mojom.EyeDropperChooser);
    this.interceptor_ =
        new MojoInterfaceInterceptor(blink.mojom.EyeDropperChooser.name);
    this.interceptor_.oninterfacerequest =
        e => this.bindingSet_.addBinding(this, e.handle);
    this.interceptor_.start();

    this.bindingSet_.setConnectionErrorHandler(() => {
      this.count_--;
    });
    this.count_ = 0;
  }

  choose() {
    this.count_++;
    return new Promise((resolve, reject) => {
      // TODO(crbug.com/992297): handle value chosen.
    });
  }

  isChooserShown() {
    return this.count_ > 0;
  }
}

let mockEyeDropperChooser = new MockEyeDropperChooser();

function waitUntilEyeDropperShown(then) {
  if (!mockEyeDropperChooser.isChooserShown())
    return setTimeout(() => { waitUntilEyeDropperShown(then); }, 0);
  if (then)
    then();
}

function waitUntilEyeDropperClosed(then) {
  if (mockEyeDropperChooser.isChooserShown())
    return setTimeout(() => { waitUntilEyeDropperClosed(then); }, 0);
  if (then)
    then();
}
