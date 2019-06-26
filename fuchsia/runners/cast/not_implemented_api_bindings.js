// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stub definitions of Cast Platform API functions.

if (!cast)
  var cast = {};

if (!cast.__platform__)
  cast.__platform__ = {};

// Don't clobber the API stubs if they were previously injected.
if (!cast.__platform__._notImplemented) {

  // Determines how a value is returned to the caller of a stub function.
  cast.__platform__.ReturnType = {
    // The value is returned immediately by the function.
    RETURN_VALUE: 0,

    // The value is returned as a resolved Promise.
    PROMISE_RESOLVED: 1,

    // The value is returned as a rejected Promise.
    PROMISE_REJECTED: 2,

    // The value is passed as an argument list to the callback provided by the
    // caller.
    CALLBACK: 3
  };

  // Returns a stub function that logs messages to the console when called, and
  // optionally returns a dummy value to the caller.
  //
  // |returnType|: specifies the mechanism for how |returnValue| is passed back
  // to the caller.
  cast.__platform__._notImplemented = function(functionName, returnValue,
                                               returnType) {
    return function(...args) {
      console.log('Unimplemented stub function called: cast.__platform__.' +
          functionName + '(' + args.map(JSON.stringify).join(', ') + ')');

      if (!returnType ||
          returnType == cast.__platform__.ReturnType.RETURN_VALUE) {
        console.log('Returning ' + JSON.stringify(returnValue));
        return returnValue;
      } else if (returnType == cast.__platform__.ReturnType.PROMISE_RESOLVED) {
        console.log('Returning promise ' + JSON.stringify(returnValue));
        return Promise.resolve(returnValue);
      } else if (returnType == cast.__platform__.ReturnType.PROMISE_REJECTED) {
        console.log('Returning rejected promise ' +
            JSON.stringify(returnValue));
        return Promise.reject(returnValue);
      } else if (returnType == cast.__platform__.ReturnType.CALLBACK) {
        console.log('Returning via callback ' + JSON.stringify(returnValue));
        args[0].apply(window, returnValue);
      }
    }
  };


  if (!cast.__platform__.canDisplayType) {
    cast.__platform__.canDisplayType =
        cast.__platform__._notImplemented('canDisplayType', true);
  }

  if (!cast.__platform__.setAssistantMessageHandler) {
    cast.__platform__.setAssistantMessageHandler =
        cast.__platform__._notImplemented('setAssistantMessageHandler');
  }

  if (!cast.__platform__.sendAssistantRequest) {
    cast.__platform__.sendAssistantRequest =
        cast.__platform__._notImplemented('sendAssistantRequest');
  }

  if (!cast.__platform__.setWindowRequestHandler) {
    cast.__platform__.setWindowRequestHandler =
        cast.__platform__._notImplemented('setWindowRequestHandler');
  }

  if (!cast.__platform__.takeScreenshot) {
    cast.__platform__.takeScreenshot =
        cast.__platform__._notImplemented('takeScreenshot');
  }


  if (!cast.__platform__.crypto) {
    cast.__platform__.crypto = {};

    cast.__platform__.crypto.encrypt =
        cast.__platform__._notImplemented(
            'crypto.encrypt',
            new ArrayBuffer(0),
            cast.__platform__.ReturnType.PROMISE_RESOLVED);

    cast.__platform__.crypto.decrypt =
        cast.__platform__._notImplemented(
            'crypto.decrypt',
            new ArrayBuffer(0),
            cast.__platform__.ReturnType.PROMISE_RESOLVED);

    cast.__platform__.crypto.sign =
        cast.__platform__._notImplemented(
            'crypto.sign',
            new ArrayBuffer(0),
            cast.__platform__.ReturnType.PROMISE_RESOLVED);

    cast.__platform__.crypto.unwrapKey =
        cast.__platform__._notImplemented(
            'crypto.unwrapKey',
            undefined,
            cast.__platform__.ReturnType.PROMISE_REJECTED);

    cast.__platform__.crypto.verify =
        cast.__platform__._notImplemented(
            'crypto.verify',
            true,
            cast.__platform__.ReturnType.PROMISE_RESOLVED);

    cast.__platform__.crypto.wrapKey =
        cast.__platform__._notImplemented(
            'crypto.wrapKey',
            undefined,
            cast.__platform__.ReturnType.PROMISE_REJECTED);
  }


  if (!cast.__platform__.cryptokeys) {
    cast.__platform__.cryptokeys = {};

    cast.__platform__.cryptokeys.getKeyByName =
        cast.__platform__._notImplemented(
            'cryptokeys.getKeyByName',
            '',
            cast.__platform__.ReturnType.PROMISE_REJECTED);
  }


  if (!cast.__platform__.display) {
    cast.__platform__.display = {};

    cast.__platform__.display.updateOutputMode =
        cast.__platform__._notImplemented(
            'display.updateOutputMode',
            Promise.resolve(),
            cast.__platform__.ReturnType.PROMISE_RESOLVED);

    cast.__platform__.display.getHdcpVersion =
        cast.__platform__._notImplemented(
            'display.getHdcpVersion',
            '0',
            cast.__platform__.ReturnType.PROMISE_RESOLVED);
  }


  if (!cast.__platform__.accessibility) {
    cast.__platform__.accessibility = {};

    cast.__platform__.accessibility.getAccessibilitySettings =
        cast.__platform__._notImplemented(
            'accessibility.getAccessibilitySettings',
            {isColorInversionEnabled: false, isScreenReaderEnabled: false},
            cast.__platform__.ReturnType.PROMISE_RESOLVED);

    cast.__platform__.accessibility.setColorInversion =
        cast.__platform__._notImplemented(
            'accessibility.setColorInversion');

    cast.__platform__.accessibility.setMagnificationGesture =
        cast.__platform__._notImplemented(
            'accessibility.setMagnificationGesture');
  }


  if (!cast.__platform__.windowManager) {
    cast.__platform__.windowManager = {};

    cast.__platform__.windowManager.onBackGesture =
        cast.__platform__._notImplemented('windowManager.onBackGesture',
            undefined,
            cast.__platform__.ReturnType.CALLBACK);

    cast.__platform__.windowManager.onBackGestureProgress =
        cast.__platform__._notImplemented('windowManager.onBackGestureProgress',
            [0],
            cast.__platform__.ReturnType.CALLBACK);

    cast.__platform__.windowManager.onBackGestureCancel =
        cast.__platform__._notImplemented('windowManager.onBackGestureCancel',
            undefined,
            cast.__platform__.ReturnType.CALLBACK);

    cast.__platform__.windowManager.onTopDragGestureDone =
        cast.__platform__._notImplemented('windowManager.onTopDragGestureDone',
            undefined,
            cast.__platform__.ReturnType.CALLBACK);

    cast.__platform__.windowManager.onTopDragGestureProgress =
        cast.__platform__._notImplemented(
            'windowManager.onTopDragGestureProgress',
            [0],
            cast.__platform__.ReturnType.CALLBACK);

    cast.__platform__.windowManager.onTapGesture =
        cast.__platform__._notImplemented('windowManager.onTapGesture',
            undefined,
            cast.__platform__.ReturnType.CALLBACK);

    cast.__platform__.windowManager.onTapDownGesture =
        cast.__platform__._notImplemented('windowManager.onTapDownGesture',
            undefined,
            cast.__platform__.ReturnType.CALLBACK);

    cast.__platform__.windowManager.canTopDrag =
        cast.__platform__._notImplemented('windowManager.canTopDrag', false);

    cast.__platform__.windowManager.canGoBack =
        cast.__platform__._notImplemented('windowManager.canGoBack', false);

    cast.__platform__.windowManager.onRightDragGestureDone =
        cast.__platform__._notImplemented(
            'windowManager.onRightDragGestureDone',
            undefined,
            cast.__platform__.ReturnType.CALLBACK);

    cast.__platform__.windowManager.onRightDragGestureProgress =
        cast.__platform__._notImplemented(
            'windowManager.onRightDragGestureProgress',
            [0, 0],
            cast.__platform__.ReturnType.CALLBACK);

    cast.__platform__.windowManager.canRightDrag =
        cast.__platform__._notImplemented(
            'windowManager.canRightDrag', false);
  }
}
