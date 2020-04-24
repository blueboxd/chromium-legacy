// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Host-side of web-driver like controller for sandboxed guest frames. */
class GuestDriver {
  /** @param {string} origin */
  constructor(origin) {
    this.origin = origin;

    /** @type {Array<MessageEvent<TestMessageResponseData>>} */
    this.testMessageQueue = [];

    /** @type {?function(MessageEvent<TestMessageResponseData>)} */
    this.testMessageWaiter = null;

    this.messageListener = (/** Event */ event) => {
      const testEvent =
          /** @type{MessageEvent<TestMessageResponseData>} */ (event);
      if (!testEvent.data.testQueryResult) {
        // Not a message for the driver.
        return;
      }

      console.log('Event from guest: ' + JSON.stringify(testEvent.data));
      if (this.testMessageWaiter) {
        this.testMessageWaiter(testEvent);
        this.testMessageWaiter = null;
      } else {
        this.testMessageQueue.push(testEvent);
      }
    };

    window.addEventListener('message', this.messageListener);
  }

  tearDown() {
    window.removeEventListener('message', this.messageListener);
  }

  /**
   * Returns the next message from the guest.
   * @return {Promise<MessageEvent<TestMessageResponseData>>}
   */
  popTestMessage() {
    if (this.testMessageQueue.length > 0) {
      const front = this.testMessageQueue[0];
      this.testMessageQueue.shift();
      return Promise.resolve(front);
    }
    return new Promise(resolve => {
      this.testMessageWaiter = resolve;
    });
  }

  /**
   * Sends a query to the guest that runs postMessage in the guest context.
   *
   * @param {string} query the message to post in the guest.
   * @param {string=} data additional data to post with the query.
   * @return {Promise<string>} JSON.stringify()'d value of response if there is
   *  one.
   */
  async sendPostMessageRequest(query, data = '') {
    const frame = assertInstanceof(
        document.querySelector(`iframe[src^="${this.origin}"]`),
        HTMLIFrameElement);
    /** @type{TestMessageQueryData} */
    const message = {testQuery: query, testData: data};
    frame.contentWindow.postMessage(message, this.origin);
    const event = await this.popTestMessage();
    return event.data.testQueryResult;
  }

  /**
   * Runs the given `testCase` in the guest context.
   * @param {string} testCase
   */
  async runTestInGuest(testCase) {
    await this.sendPostMessageRequest('run-test-case', testCase);
  }
}
