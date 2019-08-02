// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Don't supercede an Agent-provided API function.
if (!cast.__platform__.setTouchInputSupport) {
  cast.__platform__.__touchInput__ = new class {
    constructor() {
      this.port_ = cast.__platform__.PortConnector.bind(
          'cast.__platform__.__touchInput__');

      this.port_.onmessage = function(message) {
            var responseParsed = JSON.parse(message.data);
            if (responseParsed) {
              this.onAck(responseParsed.requestId,
                  responseParsed.displayControls);
            }
          }.bind(this);
    }

    // Receives an acknowledgement from the native bindings layer and relays the
    // result to the Promise.
    onAck(requestId, displayControls) {
      if (!this.pendingRequests_.hasOwnProperty(requestId)) {
        console.error('Received ack for unknown request ID: ' + requestId);
        return;
      }

      var request = this.pendingRequests_[requestId];
      delete this.pendingRequests_[requestId];

      if (displayControls === undefined) {
        request.reject();
      } else {
        request.resolve({'displayControls': displayControls});
      }
    }

    // Requests touch input support from the native bindings layer and returns a
    // Promise with the result.
    // If the request was successful, the Promise will be resolved with a
    // dictionary indicating whether onscreen controls should be shown for the
    // application.
    // If the request was rejected, then the Promise will be rejected.
    setTouchInputSupport(touchEnabled) {
      return new Promise((resolve, reject) => {
        var requestId = this.currentRequestId_++;
        this.pendingRequests_[requestId] = {
          'resolve': resolve,
          'reject': reject,
        };
        this.port_.postMessage(JSON.stringify({
          'requestId': requestId,
          'touchEnabled': touchEnabled,
        }));
      });
    }

    // Port for sending requests and receiving acknowledgements with the native
    // bindings layer.
    port_ = null;

    // A dictionary relating inflight request IDs to their Promise
    // resolve/reject functions.
    pendingRequests_ = {};

    // Unique ID of the next request.
    currentRequestId_ = 0;
  };

  cast.__platform__.setTouchInputSupport =
      cast.__platform__.__touchInput__.setTouchInputSupport.bind(
          cast.__platform__.__touchInput__);
}
