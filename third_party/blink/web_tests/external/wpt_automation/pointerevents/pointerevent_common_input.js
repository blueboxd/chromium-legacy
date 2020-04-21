// This file contains the commonly used functions in pointerevent tests.

const scrollOffset = 20;
const boundaryOffset = 2;

function scrollPageIfNeeded(targetSelector, targetDocument) {
  var target = targetDocument.querySelector(targetSelector);
  var targetRect = target.getBoundingClientRect();
  if (targetRect.top < 0 || targetRect.left < 0 || targetRect.bottom > window.innerHeight || targetRect.right > window.innerWidth)
    window.scrollTo(targetRect.left, targetRect.top);
}

function waitForCompositorCommit() {
  return new Promise((resolve) => {
    // For now, we just rAF twice. It would be nice to have a proper mechanism
    // for this.
    window.requestAnimationFrame(() => {
      window.requestAnimationFrame(resolve);
    });
  });
}

// Mouse inputs.
function mouseMoveToDocument() {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      chrome.gpuBenchmarking.pointerActionSequence(
          [{
            source: 'mouse',
            actions: [{name: 'pointerMove', x: 0, y: 0}]
          }],
          resolve);
    } else {
      reject();
    }
  });
}

function mouseMoveIntoTarget(targetSelector, targetFrame) {
  var targetDocument = document;
  var frameLeft = 0;
  var frameTop = 0;
  if (targetFrame !== undefined) {
    targetDocument = targetFrame.contentDocument;
    var frameRect = targetFrame.getBoundingClientRect();
    frameLeft = frameRect.left;
    frameTop = frameRect.top;
  }
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      scrollPageIfNeeded(targetSelector, targetDocument);
      var target = targetDocument.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition = frameLeft + targetRect.left + boundaryOffset;
      var yPosition = frameTop + targetRect.top + boundaryOffset;
      chrome.gpuBenchmarking.pointerActionSequence(
          [{
            source: 'mouse',
            actions:
                [{name: 'pointerMove', x: xPosition, y: yPosition}]
          }],
          resolve);
    } else {
      reject();
    }
  });
}

function mouseClickInTarget(targetSelector, targetFrame, button, shouldScrollToTarget = true) {
  var targetDocument = document;
  var frameLeft = 0;
  var frameTop = 0;
  // Initialize the button value to left button.
  if (button === undefined) {
    button = 0;
  }
  if (targetFrame !== undefined) {
    targetDocument = targetFrame.contentDocument;
    var frameRect = targetFrame.getBoundingClientRect();
    frameLeft = frameRect.left;
    frameTop = frameRect.top;
  }
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      if (shouldScrollToTarget)
        scrollPageIfNeeded(targetSelector, targetDocument);
      var target = targetDocument.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition = frameLeft + targetRect.left + boundaryOffset;
      var yPosition = frameTop + targetRect.top + boundaryOffset;
      chrome.gpuBenchmarking.pointerActionSequence(
          [{
            source: 'mouse',
            actions: [
              {name: 'pointerMove', x: xPosition, y: yPosition},
              {name: 'pointerDown', x: xPosition, y: yPosition, button: button},
              {name: 'pointerUp', button: button}
            ]
          }],
          resolve);
    } else {
      reject();
    }
  });
}

function mouseDragInTargets(targetSelectorList, button) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      // Initialize the button value to left button.
      if (button === undefined) {
        button = 0;
      }
      scrollPageIfNeeded(targetSelectorList[0], document);
      var target = document.querySelector(targetSelectorList[0]);
      var targetRect = target.getBoundingClientRect();
      var xPosition = targetRect.left + boundaryOffset;
      var yPosition = targetRect.top + boundaryOffset;
      var pointerActions = [{'source': 'mouse'}];
      var pointerAction = pointerActions[0];
      pointerAction.actions = [];
      pointerAction.actions.push(
          {name: 'pointerDown', x: xPosition, y: yPosition, button: button});
      for (var i = 1; i < targetSelectorList.length; i++) {
        scrollPageIfNeeded(targetSelectorList[i], document);
        target = document.querySelector(targetSelectorList[i]);
        targetRect = target.getBoundingClientRect();
        xPosition = targetRect.left + boundaryOffset;
        yPosition = targetRect.top + boundaryOffset;
        pointerAction.actions.push(
            {name: 'pointerMove', x: xPosition, y: yPosition, button: button});
      }
      pointerAction.actions.push({name: 'pointerUp', button: button});
      chrome.gpuBenchmarking.pointerActionSequence(pointerActions, resolve);
    } else {
      reject();
    }
  });
}

function mouseDragInTarget(targetSelector) {
  return mouseDragInTargets([targetSelector, targetSelector]);
}

function smoothScrollBy(scrollOffset, xPosition, yPosition, direction, source, speed, preciseScrollingDeltas) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      var scrollOffsetX = 0;
      var scrollOffsetY = 0;
      if (direction == "down") {
        scrollOffsetY = scrollOffset;
      } else if (direction == "up") {
        scrollOffsetY = -scrollOffset;
      } else if (direction == "right") {
        scrollOffsetX = scrollOffset;
      } else if (direction == "left") {
        scrollOffsetX = -scrollOffset;
      } else if (direction == "upleft") {
        scrollOffsetX = -scrollOffset;
        scrollOffsetY = -scrollOffset;
      } else if (direction == "upright") {
        scrollOffsetX = scrollOffset;
        scrollOffsetY = -scrollOffset;
      } else if (direction == "downleft") {
        scrollOffsetX = -scrollOffset;
        scrollOffsetY = scrollOffset;
      } else if (direction == "downright") {
        scrollOffsetX = scrollOffset;
        scrollOffsetY = scrollOffset;
      }
      chrome.gpuBenchmarking.smoothScrollByXY(scrollOffsetX, scrollOffsetY, resolve, xPosition,
        yPosition, source, speed, preciseScrollingDeltas);
    } else {
      reject();
    }
  });
}

function mouseWheelScroll(targetSelector, direction) {
  scrollPageIfNeeded(targetSelector, document);
  var target = document.querySelector(targetSelector);
  var targetRect = target.getBoundingClientRect();
  var xPosition = targetRect.left + boundaryOffset;
  var yPosition = targetRect.top + boundaryOffset;
  const SPEED_INSTANT = 400000;
  const PRECISE_SCROLLING_DELTAS = false;
  return smoothScrollBy(scrollOffset, xPosition, yPosition, direction, chrome.gpuBenchmarking.TOUCHPAD_INPUT, SPEED_INSTANT, PRECISE_SCROLLING_DELTAS);
}

// Touch inputs.
function touchTapInTarget(targetSelector, targetFrame) {
  var targetDocument = document;
  var frameLeft = 0;
  var frameTop = 0;
  if (targetFrame !== undefined) {
    targetDocument = targetFrame.contentDocument;
    var frameRect = targetFrame.getBoundingClientRect();
    frameLeft = frameRect.left;
    frameTop = frameRect.top;
  }
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      scrollPageIfNeeded(targetSelector, targetDocument);
      var target = targetDocument.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition = frameLeft + targetRect.left + boundaryOffset;
      var yPosition = frameTop + targetRect.top + boundaryOffset;
      chrome.gpuBenchmarking.pointerActionSequence( [
        {source: 'touch',
         actions: [
            { name: 'pointerDown', x: xPosition, y: yPosition },
            { name: 'pointerUp' }
        ]}], resolve);
    } else {
      reject();
    }
  });
}

function pointerDragInTarget(pointerType, targetSelector, direction) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      scrollPageIfNeeded(targetSelector, document);
      var target = document.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition1 = targetRect.left + boundaryOffset + scrollOffset;
      var yPosition1 = targetRect.top + boundaryOffset + scrollOffset;
      var xPosition2 = xPosition1;
      var yPosition2 = yPosition1;
      var xPosition3 = xPosition1;
      var yPosition3 = yPosition1;
      if (direction == "down") {
        yPosition1 -= scrollOffset;
        yPosition3 += scrollOffset;
      } else if (direction == "up") {
        yPosition1 += scrollOffset;
        yPosition3 -= scrollOffset;
      } else if (direction == "right") {
        xPosition1 -= scrollOffset;
        xPosition3 += scrollOffset;
      } else if (direction == "left") {
        xPosition1 += scrollOffset;
        xPosition3 -= scrollOffset;
      } else {
        throw("drag direction '" + direction + "' is not expected, direction should be 'down', 'up', 'left' or 'right'");
      }

      // Ensure the compositor is aware of any scrolling done in
      // |scrollPageIfNeeded| before sending the input events.
      waitForCompositorCommit().then(() => {
        chrome.gpuBenchmarking.pointerActionSequence( [
          {source: pointerType,
           actions: [
              { name: 'pointerDown', x: xPosition1, y: yPosition1 },
              { name: 'pointerMove', x: xPosition2, y: yPosition2 },
              { name: 'pointerMove', x: xPosition3, y: yPosition3 },
              { name: 'pause', duration: 100 },
              { name: 'pointerUp' }
          ]}], resolve);
      });
    } else {
      reject();
    }
  });
}

function touchScrollInTarget(targetSelector, direction) {
  if (direction == "down")
    direction = "up";
  else if (direction == "up")
    direction = "down";
  else if (direction == "right")
    direction = "left";
  else if (direction == "left")
    direction = "right";
  return pointerDragInTarget('touch', targetSelector, direction);
}

function pinchZoomInTarget(targetSelector, scale) {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      scrollPageIfNeeded(targetSelector, document);
      var target = document.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition = targetRect.left + (targetRect.width/2);
      var yPosition1 = targetRect.top + (targetRect.height/2) - 10;
      var yPosition2 = targetRect.top + (targetRect.height/2) + 10;
      var pointerActions = [{'source': 'touch'}, {'source': 'touch'}];
      var pointerAction1 = pointerActions[0];
      var pointerAction2 = pointerActions[1];
      pointerAction1.actions = [];
      pointerAction2.actions = [];
      pointerAction1.actions.push(
          {name: 'pointerDown', x: xPosition, y: yPosition1});
      pointerAction2.actions.push(
          {name: 'pointerDown', x: xPosition, y: yPosition2});
      for (var offset = 10; offset < 80; offset += 10) {
        pointerAction1.actions.push({
          name: 'pointerMove',
          x: xPosition,
          y: (yPosition1 - offset)
        });
        pointerAction2.actions.push({
          name: 'pointerMove',
          x: xPosition,
          y: (yPosition2 + offset)
        });
      }
      pointerAction1.actions.push({name: 'pointerUp'});
      pointerAction2.actions.push({name: 'pointerUp'});
      // Ensure the compositor is aware of any scrolling done in
      // |scrollPageIfNeeded| before sending the input events.
      waitForCompositorCommit().then(() => {
        chrome.gpuBenchmarking.pointerActionSequence(pointerActions, resolve);
      });
    } else {
      reject();
    }
  });
}

// Pen inputs.
function penMoveToDocument() {
  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      chrome.gpuBenchmarking.pointerActionSequence( [
        {source: 'pen',
         actions: [
            { name: 'pointerMove', x: 0, y: 0 }
        ]}], resolve);
    } else {
      reject();
    }
  });
}

function penEnterAndLeaveTarget(targetSelector, targetFrame) {
  var targetDocument = document;
  var frameLeft = 0;
  var frameTop = 0;
  if (targetFrame !== undefined) {
    targetDocument = targetFrame.contentDocument;
    var frameRect = targetFrame.getBoundingClientRect();
    frameLeft = frameRect.left;
    frameTop = frameRect.top;
  }

  return new Promise(function(resolve, reject) {
    if (window.chrome && chrome.gpuBenchmarking) {
      var target = targetDocument.querySelector(targetSelector);
      var targetRect = target.getBoundingClientRect();
      var xPosition = frameLeft + targetRect.left + boundaryOffset;
      var yPosition = frameTop + targetRect.top + boundaryOffset;
      chrome.gpuBenchmarking.pointerActionSequence( [
        {source: 'pen',
         actions: [
            { name: 'pointerMove', x: xPosition, y: yPosition},
            { name: 'pointerLeave' },
        ]}], resolve);
    } else {
      reject();
    }
  });
}

// Drag and drop actions
function mouseDragAndDropInTargets(targetSelectorList) {
  return new Promise(function(resolve, reject) {
    if (window.eventSender) {
      scrollPageIfNeeded(targetSelectorList[0], document);
      var target = document.querySelector(targetSelectorList[0]);
      var targetRect = target.getBoundingClientRect();
      var xPosition = targetRect.left + boundaryOffset;
      var yPosition = targetRect.top + boundaryOffset;
      eventSender.mouseMoveTo(xPosition, yPosition);
      eventSender.mouseDown();
      eventSender.leapForward(100);
      for (var i = 1; i < targetSelectorList.length; i++) {
        scrollPageIfNeeded(targetSelectorList[i], document);
        target = document.querySelector(targetSelectorList[i]);
        targetRect = target.getBoundingClientRect();
        xPosition = targetRect.left + boundaryOffset;
        yPosition = targetRect.top + boundaryOffset;
        eventSender.mouseMoveTo(xPosition, yPosition);
      }
      eventSender.mouseUp();
      resolve();
    } else {
      reject();
    }
  });
}

{
  var pointerevent_automation = async_test("PointerEvent Automation");
  // Defined in every test and should return a promise that gets resolved when input is finished.
  inject_input().then(function() {
    pointerevent_automation.done();
  });
}
