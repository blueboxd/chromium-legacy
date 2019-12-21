// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Touch {
  constructor() {
    this.points = {};
  }

  /** @private */
  lowestID_() {
    let ans = -1;
    for (const key in this.points) {
      ans = Math.max(ans, key);
    }
    return ans + 1;
  }

  updateTouchPoint(key, x, y, offsetX, offsetY) {
    const e = {clientX: x, clientY: y, pageX: x, pageY: y};
    if (typeof (offsetX) === 'number') {
      e.clientX += offsetX;
    }
    if (typeof (offsetY) === 'number') {
      e.clientY += offsetY;
    }
    this.points[key] = e;
  }

  addTouchPoint(x, y, offsetX, offsetY) {
    this.updateTouchPoint(this.lowestID_(), x, y, offsetX, offsetY);
  }

  releaseTouchPoint(key) {
    delete this.points[key];
  }

  events() {
    const arr = [];
    for (const key in this.points) {
      arr.push(this.points[key]);
    }
    return {touches: arr, preventDefault: function() {}};
  }
}

class PinchTest {
  /** @private */
  assertTrue_(condition, message) {
    if (!condition) {
      message = message || 'Assertion failed';
      console.trace();
      throw new Error(message);
    }
  }

  /** @private */
  assertClose_(a, b, message) {
    if (Math.abs(a - b) > 1e-5) {
      message = message || 'Assertion failed';
      console.log('"', a, '" and "', b, '" are not close.');
      console.trace();
      throw new Error(message);
    }
  }

  /** @private */
  isEquivalent_(a, b) {
    // Create arrays of property names
    const aProps = Object.getOwnPropertyNames(a);
    const bProps = Object.getOwnPropertyNames(b);

    // If number of properties is different,
    // objects are not equivalent
    if (aProps.length != bProps.length) {
      return false;
    }

    for (let i = 0; i < aProps.length; i++) {
      const propName = aProps[i];

      // If values of same property are not equal,
      // objects are not equivalent
      if (a[propName] !== b[propName]) {
        return false;
      }
    }

    // If we made it this far, objects
    // are considered equivalent
    return true;
  }

  /** @private */
  assertEqual_(a, b, message) {
    if (!this.isEquivalent_(a, b)) {
      message = message || 'Assertion failed';
      console.log('"', a, '" and "', b, '" are not equal');
      console.trace();
      throw new Error(message);
    }
  }

  testZoomOut() {
    pincher.reset();
    const t = new Touch();

    // Make sure start event doesn't change state
    let oldState = pincher.status();
    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    this.assertEqual_(oldState, pincher.status());
    t.addTouchPoint(300, 300);
    pincher.handleTouchStart(t.events());
    this.assertEqual_(oldState, pincher.status());

    // Make sure extra move event doesn't change state
    pincher.handleTouchMove(t.events());
    this.assertEqual_(oldState, pincher.status());

    t.updateTouchPoint(0, 150, 150);
    t.updateTouchPoint(1, 250, 250);
    pincher.handleTouchMove(t.events());
    this.assertTrue_(pincher.status().clampedScale < 0.9);

    // Make sure end event doesn't change state
    oldState = pincher.status();
    t.releaseTouchPoint(1);
    pincher.handleTouchEnd(t.events());
    this.assertEqual_(oldState, pincher.status());
    t.releaseTouchPoint(0);
    pincher.handleTouchEnd(t.events());
    this.assertEqual_(oldState, pincher.status());
  }

  testZoomIn() {
    pincher.reset();
    const t = new Touch();

    let oldState = pincher.status();
    t.addTouchPoint(150, 150);
    pincher.handleTouchStart(t.events());
    this.assertEqual_(oldState, pincher.status());
    t.addTouchPoint(250, 250);
    pincher.handleTouchStart(t.events());
    this.assertEqual_(oldState, pincher.status());

    t.updateTouchPoint(0, 100, 100);
    t.updateTouchPoint(1, 300, 300);
    pincher.handleTouchMove(t.events());
    this.assertTrue_(pincher.status().clampedScale > 1.1);

    oldState = pincher.status();
    t.releaseTouchPoint(1);
    pincher.handleTouchEnd(t.events());
    this.assertEqual_(oldState, pincher.status());
    t.releaseTouchPoint(0);
    pincher.handleTouchEnd(t.events());
    this.assertEqual_(oldState, pincher.status());
  }

  testZoomOutAndPan() {
    pincher.reset();
    const t = new Touch();
    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(300, 300);
    pincher.handleTouchStart(t.events());
    t.updateTouchPoint(0, 150, 150);
    t.updateTouchPoint(1, 250, 250);
    pincher.handleTouchMove(t.events());
    t.updateTouchPoint(0, 150, 150, 10, -5);
    t.updateTouchPoint(1, 250, 250, 10, -5);
    pincher.handleTouchMove(t.events());
    t.releaseTouchPoint(1);
    pincher.handleTouchEnd(t.events());
    t.releaseTouchPoint(0);
    pincher.handleTouchEnd(t.events());

    this.assertClose_(pincher.status().shiftX, 10);
    this.assertClose_(pincher.status().shiftY, -5);
    this.assertTrue_(pincher.status().clampedScale < 0.9);
  }

  testReversible() {
    pincher.reset();
    const t = new Touch();
    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(300, 300);
    pincher.handleTouchStart(t.events());
    t.updateTouchPoint(0, 0, 0);
    t.updateTouchPoint(1, 400, 400);
    pincher.handleTouchMove(t.events());
    t.releaseTouchPoint(1);
    pincher.handleTouchEnd(t.events());
    t.releaseTouchPoint(0);
    pincher.handleTouchEnd(t.events());
    t.addTouchPoint(0, 0);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(400, 400);
    pincher.handleTouchStart(t.events());
    t.updateTouchPoint(0, 100, 100);
    t.updateTouchPoint(1, 300, 300);
    pincher.handleTouchMove(t.events());
    t.releaseTouchPoint(1);
    pincher.handleTouchEnd(t.events());
    t.releaseTouchPoint(0);
    pincher.handleTouchEnd(t.events());
    this.assertClose_(pincher.status().clampedScale, 1);
  }

  testMultitouchZoomOut() {
    pincher.reset();
    const t = new Touch();

    let oldState = pincher.status();
    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    this.assertEqual_(oldState, pincher.status());
    t.addTouchPoint(300, 300);
    pincher.handleTouchStart(t.events());
    this.assertEqual_(oldState, pincher.status());
    t.addTouchPoint(100, 300);
    pincher.handleTouchStart(t.events());
    this.assertEqual_(oldState, pincher.status());
    t.addTouchPoint(300, 100);
    pincher.handleTouchStart(t.events());
    this.assertEqual_(oldState, pincher.status());

    // Multi-touch zoom out.
    t.updateTouchPoint(0, 150, 150);
    t.updateTouchPoint(1, 250, 250);
    t.updateTouchPoint(2, 150, 250);
    t.updateTouchPoint(3, 250, 150);
    pincher.handleTouchMove(t.events());

    oldState = pincher.status();
    t.releaseTouchPoint(3);
    pincher.handleTouchEnd(t.events());
    this.assertEqual_(oldState, pincher.status());
    t.releaseTouchPoint(2);
    pincher.handleTouchEnd(t.events());
    this.assertEqual_(oldState, pincher.status());
    t.releaseTouchPoint(1);
    pincher.handleTouchEnd(t.events());
    this.assertEqual_(oldState, pincher.status());
    t.releaseTouchPoint(0);
    pincher.handleTouchEnd(t.events());
    this.assertEqual_(oldState, pincher.status());

    this.assertTrue_(pincher.status().clampedScale < 0.9);
  }

  testZoomOutThenMulti() {
    pincher.reset();
    const t = new Touch();

    let oldState = pincher.status();
    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    this.assertEqual_(oldState, pincher.status());
    t.addTouchPoint(300, 300);
    pincher.handleTouchStart(t.events());
    this.assertEqual_(oldState, pincher.status());

    // Zoom out.
    t.updateTouchPoint(0, 150, 150);
    t.updateTouchPoint(1, 250, 250);
    pincher.handleTouchMove(t.events());
    this.assertTrue_(pincher.status().clampedScale < 0.9);

    // Make sure adding and removing more point doesn't change state
    oldState = pincher.status();
    t.addTouchPoint(600, 600);
    pincher.handleTouchStart(t.events());
    this.assertEqual_(oldState, pincher.status());
    t.releaseTouchPoint(2);
    pincher.handleTouchEnd(t.events());
    this.assertEqual_(oldState, pincher.status());

    // More than two fingers.
    t.addTouchPoint(150, 250);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(250, 150);
    pincher.handleTouchStart(t.events());
    this.assertEqual_(oldState, pincher.status());

    t.updateTouchPoint(0, 100, 100);
    t.updateTouchPoint(1, 300, 300);
    t.updateTouchPoint(2, 100, 300);
    t.updateTouchPoint(3, 300, 100);
    pincher.handleTouchMove(t.events());
    this.assertClose_(pincher.status().scale, 1);

    oldState = pincher.status();
    t.releaseTouchPoint(3);
    t.releaseTouchPoint(2);
    t.releaseTouchPoint(1);
    t.releaseTouchPoint(0);
    pincher.handleTouchEnd(t.events());
    this.assertEqual_(oldState, pincher.status());
  }

  testCancel() {
    pincher.reset();
    const t = new Touch();

    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(300, 300);
    pincher.handleTouchStart(t.events());
    t.updateTouchPoint(0, 150, 150);
    t.updateTouchPoint(1, 250, 250);
    pincher.handleTouchMove(t.events());
    this.assertTrue_(pincher.status().clampedScale < 0.9);

    const oldState = pincher.status();
    t.releaseTouchPoint(1);
    t.releaseTouchPoint(0);
    pincher.handleTouchCancel(t.events());
    this.assertEqual_(oldState, pincher.status());

    t.addTouchPoint(150, 150);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(250, 250);
    pincher.handleTouchStart(t.events());
    t.updateTouchPoint(0, 100, 100);
    t.updateTouchPoint(1, 300, 300);
    pincher.handleTouchMove(t.events());
    this.assertClose_(pincher.status().clampedScale, 1);
  }

  testSingularity() {
    pincher.reset();
    const t = new Touch();

    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    t.updateTouchPoint(0, 150, 150);
    t.updateTouchPoint(1, 50, 50);
    pincher.handleTouchMove(t.events());
    this.assertTrue_(pincher.status().clampedScale > 1.1);
    this.assertTrue_(pincher.status().clampedScale < 100);
    this.assertTrue_(pincher.status().scale < 100);

    pincher.handleTouchCancel();
  }

  testMinSpan() {
    pincher.reset();
    const t = new Touch();

    t.addTouchPoint(50, 50);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(150, 150);
    pincher.handleTouchStart(t.events());
    t.updateTouchPoint(0, 100, 100);
    t.updateTouchPoint(1, 100, 100);
    pincher.handleTouchMove(t.events());
    this.assertTrue_(pincher.status().clampedScale < 0.9);
    this.assertTrue_(pincher.status().clampedScale > 0);
    this.assertTrue_(pincher.status().scale > 0);

    pincher.handleTouchCancel();
  }

  testFontScaling() {
    pincher.reset();
    useFontScaling(1.5);
    this.assertClose_(pincher.status().clampedScale, 1.5);

    let t = new Touch();

    // Start touch.
    let oldState = pincher.status();
    t.addTouchPoint(100, 100);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(300, 300);
    pincher.handleTouchStart(t.events());

    // Pinch to zoom out.
    t.updateTouchPoint(0, 150, 150);
    t.updateTouchPoint(1, 250, 250);
    pincher.handleTouchMove(t.events());

    // Verify scale is smaller.
    this.assertTrue_(
        pincher.status().clampedScale < 0.9 * oldState.clampedScale);
    pincher.handleTouchCancel();

    useFontScaling(0.8);
    this.assertClose_(pincher.status().clampedScale, 0.8);

    // Start touch.
    t = new Touch();
    oldState = pincher.status();
    t.addTouchPoint(150, 150);
    pincher.handleTouchStart(t.events());
    t.addTouchPoint(250, 250);
    pincher.handleTouchStart(t.events());

    // Pinch to zoom in.
    t.updateTouchPoint(0, 100, 100);
    t.updateTouchPoint(1, 300, 300);
    pincher.handleTouchMove(t.events());

    // Verify scale is larger.
    this.assertTrue_(
        pincher.status().clampedScale > 1.1 * oldState.clampedScale);
    pincher.handleTouchCancel();
  }

  run() {
    this.testZoomOut();
    this.testZoomIn();
    this.testZoomOutAndPan();
    this.testReversible();
    this.testMultitouchZoomOut();
    this.testZoomOutThenMulti();
    this.testCancel();
    this.testSingularity();
    this.testMinSpan();
    this.testFontScaling();
    pincher.reset();

    return true;
  }
}
const pinchtest = new PinchTest;
