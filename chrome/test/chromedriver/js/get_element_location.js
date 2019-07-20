// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getInViewPoint(rect) {
  var left = Math.max(0, rect.left);
  var right = Math.min(window.innerWidth, rect.right);
  var top = Math.max(0, rect.top);
  var bottom = Math.min(window.innerHeight, rect.bottom);

  var x = 0.5 * (left + right);
  var y = 0.5 * (top + bottom);

  return [x, y, rect.left, rect.top];
}

function inView(element) {
  var rectangles = element.getClientRects();
  if (rectangles.length === 0) {
    return false;
  }

  var elementPoint = getInViewPoint(rectangles[0]);
  if (elementPoint[0] <= 0 || elementPoint[1] <= 0 ||
      elementPoint[0] >= window.innerWidth ||
      elementPoint[1] >= window.innerHeight ||
      !document.elementsFromPoint(elementPoint[0], elementPoint[1])
                .includes(element)) {
    return false;
  }

  return true;
}

function getElementLocation(element, center) {
  // Check that node type is element.
  if (element.nodeType != 1)
    throw new Error(element + ' is not an element');

  if (!inView(element)) {
    element.scrollIntoView({behavior: "instant",
                            block: "end",
                            inline: "nearest"});
  }

  var rect = element.getClientRects()[0];
  var elementPoint = getInViewPoint(rect);
  if (center) {
    return {
        'x': elementPoint[0],
        'y': elementPoint[1]
    };
  } else {
    return {
        'x': elementPoint[2],
        'y': elementPoint[3]
    };
  }
}
