// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let inIncognito = chrome.extension.inIncognitoContext;

chrome.alarms.onAlarm.addListener(function(alarm) {
  chrome.test.assertEq(inIncognito ? 'incognito' : 'normal', alarm.name);
  chrome.test.succeed();
});

chrome.test.runTests([
  // Creates an alarm with the name of the context it was created in.
  function createAlarms() {
    const alarmName = inIncognito ? 'incognito' : 'normal';
    chrome.alarms.create(alarmName, {delayInMinutes: 0.001});
  }
]);
