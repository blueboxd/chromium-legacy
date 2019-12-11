// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for utilities.
 */
cca.util = cca.util || {};

/**
 * Gets the clockwise rotation and flip that can orient a photo to its upright
 * position.
 * @param {!Blob} blob JPEG blob that might contain EXIF orientation field.
 * @return {Promise<Object<number, boolean>>}
 */
cca.util.getPhotoOrientation = function(blob) {
  let getOrientation = new Promise((resolve, reject) => {
    let reader = new FileReader();
    reader.onload = function(event) {
      let view = new DataView(event.target.result);
      if (view.getUint16(0, false) !== 0xFFD8) {
        resolve(1);
        return;
      }
      let length = view.byteLength;
      let offset = 2;
      while (offset < length) {
        if (view.getUint16(offset + 2, false) <= 8) {
          break;
        }
        let marker = view.getUint16(offset, false);
        offset += 2;
        if (marker === 0xFFE1) {
          if (view.getUint32(offset += 2, false) !== 0x45786966) {
            break;
          }

          let little = view.getUint16(offset += 6, false) === 0x4949;
          offset += view.getUint32(offset + 4, little);
          let tags = view.getUint16(offset, little);
          offset += 2;
          for (let i = 0; i < tags; i++) {
            if (view.getUint16(offset + (i * 12), little) === 0x0112) {
              resolve(view.getUint16(offset + (i * 12) + 8, little));
              return;
            }
          }
        } else if ((marker & 0xFF00) !== 0xFF00) {
          break;
        } else {
          offset += view.getUint16(offset, false);
        }
      }
      resolve(1);
    };
    reader.readAsArrayBuffer(blob);
  });

  return getOrientation.then((orientation) => {
    switch (orientation) {
      case 1:
        return {rotation: 0, flip: false};
      case 2:
        return {rotation: 0, flip: true};
      case 3:
        return {rotation: 180, flip: false};
      case 4:
        return {rotation: 180, flip: true};
      case 5:
        return {rotation: 90, flip: true};
      case 6:
        return {rotation: 90, flip: false};
      case 7:
        return {rotation: 270, flip: true};
      case 8:
        return {rotation: 270, flip: false};
      default:
        return {rotation: 0, flip: false};
    }
  });
};

/**
 * Orients a photo to the upright orientation.
 * @param {!Blob} blob Photo as a blob.
 * @param {function(Blob)} onSuccess Success callback with the result photo as
 *     a blob.
 * @param {function()} onFailure Failure callback.
 */
cca.util.orientPhoto = function(blob, onSuccess, onFailure) {
  // TODO(shenghao): Revise or remove this function if it's no longer
  // applicable.
  let drawPhoto = function(original, orientation, onSuccess, onFailure) {
    let canvas = document.createElement('canvas');
    let context = canvas.getContext('2d');
    let canvasSquareLength = Math.max(original.width, original.height);
    canvas.width = canvasSquareLength;
    canvas.height = canvasSquareLength;

    let [centerX, centerY] = [canvas.width / 2, canvas.height / 2];
    context.translate(centerX, centerY);
    context.rotate(orientation.rotation * Math.PI / 180);
    if (orientation.flip) {
      context.scale(-1, 1);
    }
    context.drawImage(original, -original.width / 2, -original.height / 2,
        original.width, original.height);
    if (orientation.flip) {
      context.scale(-1, 1);
    }
    context.rotate(-orientation.rotation * Math.PI / 180);
    context.translate(-centerX, -centerY);

    let outputCanvas = document.createElement('canvas');
    if (orientation.rotation === 90 || orientation.rotation === 270) {
      outputCanvas.width = original.height;
      outputCanvas.height = original.width;
    } else {
      outputCanvas.width = original.width;
      outputCanvas.height = original.height;
    }
    let imageData = context.getImageData(
        (canvasSquareLength - outputCanvas.width) / 2,
        (canvasSquareLength - outputCanvas.height) / 2,
        outputCanvas.width, outputCanvas.height);
    let outputContext = outputCanvas.getContext('2d');
    outputContext.putImageData(imageData, 0, 0);

    outputCanvas.toBlob(function(blob) {
      if (blob) {
        onSuccess(blob);
      } else {
        onFailure();
      }
    }, 'image/jpeg');
  };

  cca.util.getPhotoOrientation(blob).then((orientation) => {
    if (orientation.rotation === 0 && !orientation.flip) {
      onSuccess(blob);
    } else {
      let original = document.createElement('img');
      original.onload = function() {
        drawPhoto(original, orientation, onSuccess, onFailure);
      };
      original.onerror = onFailure;
      original.src = URL.createObjectURL(blob);
    }
  });
};

/**
 * Animates the element once by applying 'animate' class.
 * @param {HTMLElement} element Element to be animated.
 * @param {function()=} callback Callback called on completion.
 */
cca.util.animateOnce = function(element, callback) {
  cca.util.animateCancel(element).then(() => {
    element.classList.add('animate');
    cca.util.waitAnimationCompleted(element).finally(() => {
      element.classList.remove('animate');
      if (callback) {
        callback();
      }
    });
  });
};

/**
 * Cancels animating the element by removing 'animate' class.
 * @param {HTMLElement} element Element for canceling animation.
 * @return {!Promise} Promise resolved when ongoing animation is canceled and
 *     next animation can be safely applied.
 */
cca.util.animateCancel = function(element) {
  element.classList.remove('animate');
  element.classList.add('cancel-animate');
  /** @suppress {suspiciousCode} */
  element.offsetWidth;  // Force calculation to re-apply animation.
  element.classList.remove('cancel-animate');
  // Assumes transitioncancel, transitionend, animationend events from previous
  // animation are all cleared after requestAnimationFrame().
  return new Promise((r) => requestAnimationFrame(r));
};

/**
 * Waits for animation completed.
 * @param {!HTMLElement} element Element to be animated.
 * @return {!Promise} Promise is resolved/rejected when animation is
 *     completed/cancelled.
 */
cca.util.waitAnimationCompleted = function(element) {
  return new Promise((resolve, reject) => {
    let animationCount = 0;
    const onStart = (event) =>
        void (event.target === element && animationCount++);
    const onFinished = (event, callback) => {
      if (event.target !== element || --animationCount !== 0) {
        return;
      }
      events.forEach(([e, fn]) => element.removeEventListener(e, fn));
      callback();
    };
    const rejectWithError = () => reject(new Error('Animation is cancelled.'));
    const events = [
      ['transitionrun', onStart],
      ['animationstart', onStart],
      ['transitionend', (event) => onFinished(event, resolve)],
      ['animationend', (event) => onFinished(event, resolve)],
      ['transitioncancel', (event) => onFinished(event, rejectWithError)],
      // animationcancel is not implemented on chrome.
    ];
    events.forEach(([e, fn]) => element.addEventListener(e, fn));
  });
};

/**
 * Returns a shortcut string, such as Ctrl-Alt-A.
 * @param {Event} event Keyboard event.
 * @return {string} Shortcut identifier.
 */
cca.util.getShortcutIdentifier = function(event) {
  var identifier = (event.ctrlKey ? 'Ctrl-' : '') +
                   (event.altKey ? 'Alt-' : '') +
                   (event.shiftKey ? 'Shift-' : '') +
                   (event.metaKey ? 'Meta-' : '');
  if (event.key) {
    switch (event.key) {
      case 'ArrowLeft':
        identifier += 'Left';
        break;
      case 'ArrowRight':
        identifier += 'Right';
        break;
      case 'ArrowDown':
        identifier += 'Down';
        break;
      case 'ArrowUp':
        identifier += 'Up';
        break;
      case 'a':
      case 'p':
      case 's':
      case 'v':
      case 'r':
        identifier += event.key.toUpperCase();
        break;
      default:
        identifier += event.key;
    }
  }
  return identifier;
};

/**
 * Makes the element unfocusable by mouse.
 * @param {HTMLElement} element Element to be unfocusable.
 */
cca.util.makeUnfocusableByMouse = function(element) {
  element.addEventListener('mousedown', (event) => event.preventDefault());
};

/**
 * Checks if the window is maximized or fullscreen.
 * @return {boolean} True if maximized or fullscreen, false otherwise.
 */
cca.util.isWindowFullSize = function() {
  // App-window's isFullscreen, isMaximized state and window's outer-size may
  // not be updated immediately during resizing. Use if app-window's outerBounds
  // width matches screen width here as workarounds.
  return chrome.app.window.current().outerBounds.width >= screen.width ||
      chrome.app.window.current().outerBounds.height >= screen.height;
};

/**
 * Opens help.
 */
cca.util.openHelp = function() {
  window.open(
      'https://support.google.com/chromebook/?p=camera_usage_on_chromebook');
};

/**
 * Sets up i18n messages on DOM subtree by i18n attributes.
 * @param {!HTMLElement} rootElement Root of DOM subtree to be set up with.
 */
cca.util.setupI18nElements = function(rootElement) {
  var getElements = (attr) => rootElement.querySelectorAll('[' + attr + ']');
  var getMessage = (element, attr) =>
      chrome.i18n.getMessage(element.getAttribute(attr));
  var setAriaLabel = (element, attr) =>
      element.setAttribute('aria-label', getMessage(element, attr));

  getElements('i18n-content')
      .forEach(
          (element) => element.textContent =
              getMessage(element, 'i18n-content'));
  getElements('i18n-tooltip-true')
      .forEach(
          (element) => element.setAttribute(
              'tooltip-true', getMessage(element, 'i18n-tooltip-true')));
  getElements('i18n-tooltip-false')
      .forEach(
          (element) => element.setAttribute(
              'tooltip-false', getMessage(element, 'i18n-tooltip-false')));
  getElements('i18n-aria')
      .forEach((element) => setAriaLabel(element, 'i18n-aria'));
  cca.tooltip.setup(getElements('i18n-label'))
      .forEach((element) => setAriaLabel(element, 'i18n-label'));
};

/**
 * Reads blob into Image.
 * @param {!Blob} blob
 * @return {!Promise<!HTMLImageElement>}
 * @throws {Error}
 */
cca.util.blobToImage = function(blob) {
  return new Promise((resolve, reject) => {
    const img = new Image();
    img.onload = () => resolve(img);
    img.onerror = () => reject(new Error('Failed to load unprocessed image'));
    img.src = URL.createObjectURL(blob);
  });
};

/**
 * Gets default facing according to device mode.
 * @return {!Promise<string>}
 */
cca.util.getDefaultFacing = async function() {
  return await cca.mojo.ChromeHelper.getInstance().isTabletMode() ?
      'environment' :
      'user';
};

/**
 * Scales the input picture to target width and height with respect to original
 * aspect ratio.
 * @param {string} url Picture as an URL.
 * @param {boolean} isVideo Picture is a video.
 * @param {number} width Target width to be scaled to.
 * @param {number=} height Target height to be scaled to. In default, set to
 *     corresponding rounded height with respect to target width and aspect
 *     ratio of input picture.
 * @return {!Promise<!Blob>} Promise for the result.
 * @private
 */
cca.util.scalePicture = function(url, isVideo, width, height = undefined) {
  const element = document.createElement(isVideo ? 'video' : 'img');
  if (isVideo) {
    element.preload = 'auto';
  }
  return new Promise((resolve, reject) => {
           element.addEventListener(isVideo ? 'canplay' : 'load', resolve);
           element.addEventListener('error', reject);
           element.src = url;
         })
      .then(() => {
        const canvas = document.createElement('canvas');
        const context = canvas.getContext('2d');
        if (height === undefined) {
          const ratio = isVideo ? element.videoHeight / element.videoWidth :
                                  element.height / element.width;
          height = Math.round(width * ratio);
        }
        canvas.width = width;
        canvas.height = height;
        context.drawImage(element, 0, 0, width, height);
        return new Promise((resolve, reject) => {
          canvas.toBlob((blob) => {
            if (blob) {
              resolve(blob);
            } else {
              reject(new Error('Failed to create thumbnail.'));
            }
          }, 'image/jpeg');
        });
      });
};

/**
 * Toggle checked value of element.
 * @param {!HTMLInputElement} element
 * @param {boolean} checked
 */
cca.util.toggleChecked = function(element, checked) {
  element.checked = checked;
  element.dispatchEvent(new Event('change'));
};
