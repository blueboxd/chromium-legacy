// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The script runs commands against the PIN API that it receives from the C++
// side.

const SIGN_REQUEST_ID = 123;

let pinRequestCount = 0;
let pinRequestStopCount = 0;

function requestPin(requestDetails) {
  const pinRequestId = ++pinRequestCount;
  chrome.certificateProvider.requestPin(requestDetails, responseDetails => {
    reportPinRequestEnded(pinRequestId, responseDetails);
    fetchTestCommands();
  });
  reportPinRequestBegun(pinRequestId);
}

function stopPinRequest(details) {
  const pinRequestStopId = ++pinRequestStopCount;
  chrome.certificateProvider.stopPinRequest(details, () => {
    reportStopPinRequestEnded(pinRequestStopId);
    fetchTestCommands();
  });
  reportStopPinRequestBegun(pinRequestStopId);
}

function reportPinRequestBegun(pinRequestId) {
  chrome.test.sendMessage('request' + pinRequestId + ':begun');
}

function reportPinRequestEnded(pinRequestId, responseDetails) {
  let dataToSend = 'request' + pinRequestId;
  if (responseDetails)
    dataToSend += ':success:' + responseDetails.userInput;
  else if (chrome.runtime.lastError)
    dataToSend += ':error:' + chrome.runtime.lastError.message;
  else
    dataToSend += ':empty';
  chrome.test.sendMessage(dataToSend);
}

function reportStopPinRequestBegun(pinRequestStopId) {
  chrome.test.sendMessage('stop' + pinRequestStopId + ':begun');
}

function reportStopPinRequestEnded(pinRequestStopId) {
  let dataToSend = 'stop' + pinRequestStopId;
  if (chrome.runtime.lastError)
    dataToSend += ':error:' + chrome.runtime.lastError.message;
  else
    dataToSend += ':success';
  chrome.test.sendMessage(dataToSend);
}

function processTestCommand(command) {
  switch (command) {
    case 'Request':
      requestPin({signRequestId: SIGN_REQUEST_ID});
      break;
    case 'RequestWithZeroAttempts':
      requestPin({signRequestId: SIGN_REQUEST_ID, attemptsLeft: 0});
      break;
    case 'RequestWithNegativeAttempts':
      requestPin({signRequestId: SIGN_REQUEST_ID, attemptsLeft: -1});
      break;
    case 'Stop':
      stopPinRequest({signRequestId: SIGN_REQUEST_ID});
      break;
    case 'StopWithUnknownError':
      stopPinRequest(
          {signRequestId: SIGN_REQUEST_ID, errorType: 'UNKNOWN_ERROR'});
      break;
    default:
      chrome.test.fail();
  }
}

function fetchTestCommands() {
  chrome.test.sendMessage('GetCommand', command => {
    if (!command)
      return;
    processTestCommand(command);
    fetchTestCommands();
  });
}

fetchTestCommands();
