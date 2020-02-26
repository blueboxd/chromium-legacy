// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let driverSource = undefined;

function actionQuery(/** MessageEvent<TestMessageQueryData> */ event) {
  window.parent.postMessage(event.data.testQuery, 'chrome://help-app');
}

function receiveTestMessage(/** Event */ e) {
  const event = /** @type{MessageEvent<TestMessageQueryData>} */ (e);
  if (event.data.testQuery) {
    // This is a message from the test driver. Action it.
    driverSource = event.source;
    actionQuery(event);
  } else if (driverSource) {
    // This is not a message from the test driver. Report it back to the driver.
    const response = {testQueryResult: event.data};
    driverSource.postMessage(response, '*');
  }
}

window.addEventListener('message', receiveTestMessage, false);

//# sourceURL=guest_query_reciever.js
