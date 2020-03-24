// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const expectedMessages = ['Opening version…', 'This is a test status.'];

function actionsChangedListener(actions) {
  chrome.test.assertEq(2, actions.length);
  chrome.test.assertEq('Action 0', actions[0].name);
  chrome.test.assertEq(0, actions[0].index);
  chrome.test.assertEq('Action 1', actions[1].name);
  chrome.test.assertEq(1, actions[1].index);

  // Now we trigger 'Action 1' and expect the status message to change.
  chrome.autofillAssistantPrivate.performAction(1);

  // Ignore subsequent updates since they just mean a script got started and the
  // available scripts reset to the empty list.
  chrome.autofillAssistantPrivate.onActionsChanged.removeListener(
      actionsChangedListener);
}

let messageCount = 0;
function setStatusMessage(message) {
  chrome.test.assertEq(expectedMessages[messageCount++], message);

  // Everything worked as expected, notify the test infra.
  if (messageCount >= 2)
    chrome.test.succeed();
}

function testNoController() {
  chrome.autofillAssistantPrivate.start({}, () => {
    chrome.test.assertLastError('Starting requires a valid controller!');
    chrome.autofillAssistantPrivate.performAction(0, () => {
      chrome.test.assertLastError('performAction requires a valid controller!');
      chrome.test.succeed();
    });
  });
}

function testNormalFlow() {
  messageCount = 0;
  chrome.autofillAssistantPrivate.create();

  // Add listeners.
  chrome.autofillAssistantPrivate.onActionsChanged.addListener(
      actionsChangedListener);
  chrome.autofillAssistantPrivate.onStatusMessageChanged.addListener(
      setStatusMessage);

  chrome.autofillAssistantPrivate.start({}, () => {
    chrome.test.assertNoLastError();
  });
}

chrome.test.runTests([testNoController, testNormalFlow]);
