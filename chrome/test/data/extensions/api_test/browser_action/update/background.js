// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function updateBrowserAction() {
  chrome.browserAction.setTitle({title: 'Modified'});
  chrome.browserAction.setIcon({path: 'icon2.png'});
  chrome.browserAction.setBadgeText({text: 'badge'});
  chrome.browserAction.setBadgeBackgroundColor({color: [255,255,255,255]});
}

chrome.extension.isAllowedIncognitoAccess(function(isAllowedAccess) {
  if (isAllowedAccess == true) {
    chrome.test.sendMessage('incognito ready', function(message) {
      if (message == 'incognito update') {
        updateBrowserAction();
        chrome.test.notifyPass();
      }
    });
  }
});

chrome.test.sendMessage('ready', function(message) {
  if (message == 'update') {
    updateBrowserAction();
    chrome.test.notifyPass();
  }
});
