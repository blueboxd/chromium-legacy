// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On iOS, |distillerOnIos| was set to true before this script.
// eslint-disable-next-line no-var
var distillerOnIos;
if (typeof distillerOnIos === 'undefined') {
  distillerOnIos = false;
}

// The style guide recommends preferring $() to getElementById(). Chrome's
// standard implementation of $() is imported from chrome://resources, which the
// distilled page is prohibited from accessing. A version of it is
// re-implemented here to allow stylistic consistency with other JS code.
function $(id) {
  return document.getElementById(id);
}

function addToPage(html) {
  const div = document.createElement('div');
  div.innerHTML = html;
  $('content').appendChild(div);
  fillYouTubePlaceholders();
}

function fillYouTubePlaceholders() {
  const placeholders = document.getElementsByClassName('embed-placeholder');
  for (let i = 0; i < placeholders.length; i++) {
    if (!placeholders[i].hasAttribute('data-type') ||
        placeholders[i].getAttribute('data-type') !== 'youtube' ||
        !placeholders[i].hasAttribute('data-id')) {
      continue;
    }
    const embed = document.createElement('iframe');
    const url = 'http://www.youtube.com/embed/' +
        placeholders[i].getAttribute('data-id');
    embed.setAttribute('class', 'youtubeIframe');
    embed.setAttribute('src', url);
    embed.setAttribute('type', 'text/html');
    embed.setAttribute('frameborder', '0');

    const parent = placeholders[i].parentElement;
    const container = document.createElement('div');
    container.setAttribute('class', 'youtubeContainer');
    container.appendChild(embed);

    parent.replaceChild(container, placeholders[i]);
  }
}

function showLoadingIndicator(isLastPage) {
  $('loading-indicator').className = isLastPage ? 'hidden' : 'visible';
}

// Sets the title.
function setTitle(title, documentTitleSuffix) {
  $('title-holder').textContent = title;
  if (documentTitleSuffix) {
    document.title = title + documentTitleSuffix;
  } else {
    document.title = title;
  }
}

// Set the text direction of the document ('ltr', 'rtl', or 'auto').
function setTextDirection(direction) {
  document.body.setAttribute('dir', direction);
}

// These classes must agree with the font classes in distilledpage.css.
const fontFamilyClasses = ['sans-serif', 'serif', 'monospace'];
function useFontFamily(fontFamily) {
  fontFamilyClasses.forEach(
      (element) =>
          document.body.classList.toggle(element, element === fontFamily));
}

// These classes must agree with the theme classes in distilledpage.css.
const themeClasses = ['light', 'dark', 'sepia'];
function useTheme(theme) {
  themeClasses.forEach(
      (element) => document.body.classList.toggle(element, element === theme));
  updateToolbarColor(theme);
}

function getPageTheme() {
  const cls = Array.from(document.body.classList)
                  .find((cls) => themeClasses.includes(cls));
  return cls ? cls : themeClasses[0];
}

function updateToolbarColor(theme) {
  let toolbarColor;
  if (theme === 'sepia') {
    toolbarColor = '#BF9A73';
  } else if (theme === 'dark') {
    toolbarColor = '#1A1A1A';
  } else {
    toolbarColor = '#F5F5F5';
  }
  $('theme-color').content = toolbarColor;
}

function useFontScaling(scaling) {
  pincher.useFontScaling(scaling);
}

function maybeSetWebFont() {
  // On iOS, the web fonts block the rendering until the resources are
  // fetched, which can take a long time on slow networks.
  // In Blink, it times out after 3 seconds and uses fallback fonts.
  // See crbug.com/711650
  if (distillerOnIos) {
    return;
  }

  const e = document.createElement('link');
  e.href = 'https://fonts.googleapis.com/css?family=Roboto';
  e.rel = 'stylesheet';
  e.type = 'text/css';
  document.head.appendChild(e);
}

// TODO(https://crbug.com/1027612): Consider making this a custom HTML element.
class FontSizeSlider {
  constructor(element, supportedFontSizes) {
    this.element = element;
    this.supportedFontSizes = supportedFontSizes;

    this.element.addEventListener('input', (e) => {
      document.body.style.fontSize =
          this.supportedFontSizes[e.target.value] + 'px';
      this.update(e.target.value);
    });

    this.tickmarks = document.createElement('datalist');
    this.tickmarks.setAttribute('class', 'tickmarks');
    this.element.after(this.tickmarks);

    for (let i = 0; i < this.supportedFontSizes.length; i++) {
      const option = document.createElement('option');
      option.setAttribute('value', i);
      option.textContent = supportedFontSizes[i];
      this.tickmarks.appendChild(option);
    }

    this.update(this.element.value);
  }

  update(position) {
    this.element.style.setProperty(
        '--fontSizePercent',
        (position / (this.supportedFontSizes.length - 1) * 100) + '%');
    this.element.setAttribute(
        'aria-valuetext', this.supportedFontSizes[position] + 'px');
    for (let option = this.tickmarks.firstChild; option != null;
         option = option.nextSibling) {
      const isBeforeThumb = option.value < position;
      option.classList.toggle('before-thumb', isBeforeThumb);
      option.classList.toggle('after-thumb', !isBeforeThumb);
    }
  }
}

const fontSizeSlider = new FontSizeSlider(
    $('font-size-selection'), [14, 15, 16, 18, 20, 24, 28, 32, 40, 48]);

// Set the toolbar color to match the page's theme.
updateToolbarColor(getPageTheme());

maybeSetWebFont();

// The zooming speed relative to pinching speed.
const FONT_SCALE_MULTIPLIER = 0.5;

const MIN_SPAN_LENGTH = 20;

// This has to be in sync with 'font-size' in distilledpage.css.
// This value is hard-coded because JS might be injected before CSS is ready.
// See crbug.com/1004663.
const baseSize = 14;

class Pincher {
  // When users pinch in Reader Mode, the page would zoom in or out as if it
  // is a normal web page allowing user-zoom. At the end of pinch gesture, the
  // page would do text reflow. These pinch-to-zoom and text reflow effects
  // are not native, but are emulated using CSS and JavaScript.
  //
  // In order to achieve near-native zooming and panning frame rate, fake 3D
  // transform is used so that the layer doesn't repaint for each frame.
  //
  // After the text reflow, the web content shown in the viewport should
  // roughly be the same paragraph before zooming.
  //
  // The control point of font size is the html element, so that both "em" and
  // "rem" are adjusted.
  //
  // TODO(wychen): Improve scroll position when elementFromPoint is body.

  constructor() {
    this.pinching = false;
    this.fontSizeAnchor = 1.0;

    this.focusElement = null;
    this.focusPos = 0;
    this.initClientMid = null;

    this.clampedScale = 1.0;

    this.lastSpan = null;
    this.lastClientMid = null;

    this.scale = 1.0;
    this.shiftX = 0;
    this.shiftY = 0;

    window.addEventListener('touchstart', (e) => {
      this.handleTouchStart(e);
    }, {passive: false});
    window.addEventListener('touchmove', (e) => {
      this.handleTouchMove(e);
    }, {passive: false});
    window.addEventListener('touchend', (e) => {
      this.handleTouchEnd(e);
    }, {passive: false});
    window.addEventListener('touchcancel', (e) => {
      this.handleTouchCancel(e);
    }, {passive: false});
  }

  /** @private */
  refreshTransform_() {
    const slowedScale = Math.exp(Math.log(this.scale) * FONT_SCALE_MULTIPLIER);
    this.clampedScale =
        Math.max(0.5, Math.min(2.0, this.fontSizeAnchor * slowedScale));

    // Use "fake" 3D transform so that the layer is not repainted.
    // With 2D transform, the frame rate would be much lower.
    // clang-format off
    document.body.style.transform =
        'translate3d(' + this.shiftX + 'px,'
                       + this.shiftY + 'px, 0px)' +
        'scale(' + this.clampedScale / this.fontSizeAnchor + ')';
    // clang-format on
  }

  /** @private */
  saveCenter_(clientMid) {
    // Try to preserve the pinching center after text reflow.
    // This is accurate to the HTML element level.
    this.focusElement = document.elementFromPoint(clientMid.x, clientMid.y);
    const rect = this.focusElement.getBoundingClientRect();
    this.initClientMid = clientMid;
    this.focusPos =
        (this.initClientMid.y - rect.top) / (rect.bottom - rect.top);
  }

  /** @private */
  restoreCenter_() {
    const rect = this.focusElement.getBoundingClientRect();
    const targetTop = this.focusPos * (rect.bottom - rect.top) + rect.top +
        document.scrollingElement.scrollTop -
        (this.initClientMid.y + this.shiftY);
    document.scrollingElement.scrollTop = targetTop;
  }

  /** @private */
  endPinch_() {
    this.pinching = false;

    document.body.style.transformOrigin = '';
    document.body.style.transform = '';
    document.documentElement.style.fontSize =
        this.clampedScale * baseSize + 'px';

    this.restoreCenter_();

    let img = $('fontscaling-img');
    if (!img) {
      img = document.createElement('img');
      img.id = 'fontscaling-img';
      img.style.display = 'none';
      document.body.appendChild(img);
    }
    img.src = '/savefontscaling/' + this.clampedScale;
  }

  /** @private */
  touchSpan_(e) {
    const count = e.touches.length;
    const mid = this.touchClientMid_(e);
    let sum = 0;
    for (let i = 0; i < count; i++) {
      const dx = (e.touches[i].clientX - mid.x);
      const dy = (e.touches[i].clientY - mid.y);
      sum += Math.hypot(dx, dy);
    }
    // Avoid very small span.
    return Math.max(MIN_SPAN_LENGTH, sum / count);
  }

  /** @private */
  touchClientMid_(e) {
    const count = e.touches.length;
    let sumX = 0;
    let sumY = 0;
    for (let i = 0; i < count; i++) {
      sumX += e.touches[i].clientX;
      sumY += e.touches[i].clientY;
    }
    return {x: sumX / count, y: sumY / count};
  }

  /** @private */
  touchPageMid_(e) {
    const clientMid = this.touchClientMid_(e);
    return {
      x: clientMid.x - e.touches[0].clientX + e.touches[0].pageX,
      y: clientMid.y - e.touches[0].clientY + e.touches[0].pageY
    };
  }

  handleTouchStart(e) {
    if (e.touches.length < 2) {
      return;
    }
    e.preventDefault();

    const span = this.touchSpan_(e);
    const clientMid = this.touchClientMid_(e);

    if (e.touches.length > 2) {
      this.lastSpan = span;
      this.lastClientMid = clientMid;
      this.refreshTransform_();
      return;
    }

    this.scale = 1;
    this.shiftX = 0;
    this.shiftY = 0;

    this.pinching = true;
    this.fontSizeAnchor =
        parseFloat(getComputedStyle(document.documentElement).fontSize) /
        baseSize;

    const pinchOrigin = this.touchPageMid_(e);
    document.body.style.transformOrigin =
        pinchOrigin.x + 'px ' + pinchOrigin.y + 'px';

    this.saveCenter_(clientMid);

    this.lastSpan = span;
    this.lastClientMid = clientMid;

    this.refreshTransform_();
  }

  handleTouchMove(e) {
    if (!this.pinching) {
      return;
    }
    if (e.touches.length < 2) {
      return;
    }
    e.preventDefault();

    const span = this.touchSpan_(e);
    const clientMid = this.touchClientMid_(e);

    this.scale *= this.touchSpan_(e) / this.lastSpan;
    this.shiftX += clientMid.x - this.lastClientMid.x;
    this.shiftY += clientMid.y - this.lastClientMid.y;

    this.refreshTransform_();

    this.lastSpan = span;
    this.lastClientMid = clientMid;
  }

  handleTouchEnd(e) {
    if (!this.pinching) {
      return;
    }
    e.preventDefault();

    const span = this.touchSpan_(e);
    const clientMid = this.touchClientMid_(e);

    if (e.touches.length >= 2) {
      this.lastSpan = span;
      this.lastClientMid = clientMid;
      this.refreshTransform_();
      return;
    }

    this.endPinch_();
  }

  handleTouchCancel(e) {
    if (!this.pinching) {
      return;
    }
    this.endPinch_();
  }

  reset() {
    this.scale = 1;
    this.shiftX = 0;
    this.shiftY = 0;
    this.clampedScale = 1;
    document.documentElement.style.fontSize =
        this.clampedScale * baseSize + 'px';
  }

  status() {
    return {
      scale: this.scale,
      clampedScale: this.clampedScale,
      shiftX: this.shiftX,
      shiftY: this.shiftY
    };
  }

  useFontScaling(scaling) {
    this.saveCenter_({x: window.innerWidth / 2, y: window.innerHeight / 2});
    this.shiftX = 0;
    this.shiftY = 0;
    document.documentElement.style.fontSize = scaling * baseSize + 'px';
    this.clampedScale = scaling;
    this.restoreCenter_();
  }
}

const pincher = new Pincher;

class SettingsDialog {
  constructor(toggleElement, dialogElement, backdropElement) {
    this._toggleElement = toggleElement;
    this._dialogElement = dialogElement;
    this._backdropElement = backdropElement;

    this._toggleElement.addEventListener('click', this.toggle.bind(this));
    this._dialogElement.addEventListener('close', this.close.bind(this));
    this._backdropElement.addEventListener('click', this.close.bind(this));

    $('close-settings-button').addEventListener('click', this.close.bind(this));

    $('theme-selection').addEventListener('change', (e) => {
      useTheme(e.target.value);
    });

    $('font-family-selection').addEventListener('change', (e) => {
      useFontFamily(e.target.value);
    });
  }

  toggle() {
    if (this._dialogElement.open) {
      this.close();
    } else {
      this.showModal();
    }
  }

  showModal() {
    this._toggleElement.classList.add('activated');
    this._backdropElement.style.display = 'block';
    this._dialogElement.showModal();
  }

  close() {
    this._toggleElement.classList.remove('activated');
    this._backdropElement.style.display = 'none';
    this._dialogElement.close();
  }
}

const settingsDialog = new SettingsDialog(
    $('settings-toggle'), $('settings-dialog'), $('dialog-backdrop'));
