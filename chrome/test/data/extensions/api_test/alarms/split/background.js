// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let inIncognito = chrome.extension.inIncognitoContext;
let alarmTriggered = false;
let testEventFired = false;

function checkAndCompleteTest() {
  if (alarmTriggered && testEventFired)
    chrome.test.succeed();
}

chrome.alarms.onAlarm.addListener(function(alarm) {
  chrome.test.assertEq(inIncognito ? 'incognito' : 'normal', alarm.name);
  alarmTriggered = true;
  checkAndCompleteTest();
});

chrome.test.onMessage.addListener(function() {
  testEventFired = true;
  checkAndCompleteTest();
});

chrome.test.runTests([
  // Creates an alarm with the name of the context it was created in.
  function createAlarms() {
    const alarmName = inIncognito ? 'incognito' : 'normal';
    chrome.alarms.create(alarmName, {delayInMinutes: 0.001});
  }
]);

// Send a message to C++ to let it know that JS has registered event
// listeners for onAlarm and onMessage.
chrome.test.sendMessage('ready: ' + inIncognito);
