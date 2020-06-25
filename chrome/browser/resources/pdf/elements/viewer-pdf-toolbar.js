// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import 'chrome://resources/cr_elements/icons.m.js';
import './icons.js';
import './viewer-bookmark.js';
import './viewer-page-selector.js';
import './viewer-toolbar-dropdown.js';
// <if expr="chromeos">
import './viewer-pen-options.js';

// </if>

import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Bookmark} from '../bookmark_type.js';
import {SaveRequestType} from '../constants.js';

Polymer({
  is: 'viewer-pdf-toolbar',

  _template: html`{__html_template__}`,

  properties: {
    /**
     * Whether annotation mode can be entered. This would be false if for
     * example the PDF is encrypted or password protected. Note, this is
     * true regardless of whether the feature flag is enabled.
     */
    annotationAvailable: Boolean,

    /** Whether the viewer is currently in annotation mode. */
    annotationMode: {
      type: Boolean,
      notify: true,
      value: false,
      reflectToAttribute: true,
    },

    /** @type {?AnnotationTool} */
    annotationTool: {
      type: Object,
      notify: true,
    },

    /**
     * Tree of PDF bookmarks (empty if the document has no bookmarks).
     * @type {!Array<!Bookmark>}
     */
    bookmarks: {
      type: Array,
      value: () => [],
    },

    canRedoAnnotation: {
      type: Boolean,
      value: false,
    },

    canUndoAnnotation: {
      type: Boolean,
      value: false,
    },

    /** The number of pages in the PDF document. */
    docLength: Number,

    /** The title of the PDF document. */
    docTitle: String,

    hasEdits: Boolean,

    hasEnteredAnnotationMode: Boolean,

    isFormFieldFocused: {
      type: Boolean,
      observer: 'onFormFieldFocusedChanged_',
    },

    /** The current loading progress of the PDF document (0 - 100). */
    loadProgress: {
      type: Number,
      observer: 'loadProgressChanged_',
    },

    /** Whether the toolbar is opened and visible. */
    opened: {
      type: Boolean,
      value: true,
    },

    /** The number of the page being viewed (1-based). */
    pageNo: Number,

    /**
     * Whether the PDF Annotations feature is enabled.
     * @private
     */
    pdfAnnotationsEnabled_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the Printing feature is enabled.
     * @private
     */
    printingEnabled_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    downloadHasPopup_: {
      type: String,
      computed: 'computeDownloadHasPopup_(' +
          'pdfFormSaveEnabled_, hasEdits, hasEnteredAnnotationMode)',
    },

    strings: {
      type: Object,
      observer: 'onStringsSet_',
    },

    /**
     * Whether the PDF Form save feature is enabled.
     * @private
     */
    pdfFormSaveEnabled_: {
      type: Boolean,
      value: false,
    },
  },

  /** @type {?Object} */
  animation_: null,

  /** @private {?PromiseResolver<boolean>} */
  waitForFormFocusChange_: null,

  /**
   * @param {number} newProgress
   * @param {number} oldProgress
   * @private
   */
  loadProgressChanged_(newProgress, oldProgress) {
    const loaded = newProgress >= 100;
    const progressReset = newProgress < oldProgress;
    if (progressReset || loaded) {
      this.$.pageselector.classList.toggle('invisible', !loaded);
      this.$.buttons.classList.toggle('invisible', !loaded);
      this.$.progress.style.opacity = loaded ? 0 : 1;
      // <if expr="chromeos">
      this.$['annotations-bar'].hidden = !loaded || !this.annotationMode;
      // </if>
    }
  },

  hide() {
    if (this.opened && !this.shouldKeepOpen()) {
      this.toggleVisibility();
    }
  },

  show() {
    if (!this.opened) {
      this.toggleVisibility();
    }
  },

  toggleVisibility() {
    this.opened = !this.opened;

    // We keep a handle on the animation in order to cancel the filling
    // behavior of previous animations.
    if (this.animation_) {
      this.animation_.cancel();
    }

    if (this.opened) {
      this.animation_ = this.animate(
          [{transform: 'translateY(-100%)'}, {transform: 'translateY(0%)'}], {
            duration: 250,
            easing: 'cubic-bezier(0, 0, 0.2, 1)',
            fill: 'forwards',
          });
    } else {
      this.animation_ = this.animate(
          [{transform: 'translateY(0%)'}, {transform: 'translateY(-100%)'}], {
            duration: 250,
            easing: 'cubic-bezier(0.4, 0, 1, 1)',
            fill: 'forwards',
          });
    }
  },

  selectPageNumber() {
    this.$.pageselector.select();
  },

  /** @return {boolean} Whether the toolbar should be kept open. */
  shouldKeepOpen() {
    return this.$.bookmarks.dropdownOpen || this.loadProgress < 100 ||
        this.$.pageselector.isActive() || this.annotationMode ||
        this.$.downloadMenu.open;
  },

  /** @return {boolean} Whether a dropdown was open and was hidden. */
  hideDropdowns() {
    let result = false;
    if (this.$.bookmarks.dropdownOpen) {
      this.$.bookmarks.toggleDropdown();
      result = true;
    }
    // <if expr="chromeos">
    if (this.$.pen.dropdownOpen) {
      this.$.pen.toggleDropdown();
      result = true;
    }
    if (this.$.highlighter.dropdownOpen) {
      this.$.highlighter.toggleDropdown();
      result = true;
    }
    // </if>
    return result;
  },

  /** @param {number} lowerBound */
  setDropdownLowerBound(lowerBound) {
    this.$.bookmarks.lowerBound = lowerBound;
  },

  rotateRight() {
    this.fire('rotate-right');
  },

  /** @private */
  showDownloadMenu_() {
    this.$.downloadMenu.showAt(this.$.download, {
      anchorAlignmentX: AnchorAlignment.CENTER,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
      noOffset: true,
    });
    // For tests
    this.fire('download-menu-shown-for-testing');
  },

  /** @private */
  onDownloadClick_() {
    this.waitForEdits_().then(hasEdits => {
      if (hasEdits) {
        this.showDownloadMenu_();
      } else {
        this.fire('save', SaveRequestType.ORIGINAL);
      }
    });
  },

  /**
   * @return {!Promise<boolean>} Promise that resolves with true if the PDF has
   *     edits and/or annotations, and false otherwise.
   * @private
   */
  waitForEdits_() {
    if (this.hasEditsToSave_()) {
      return Promise.resolve(true);
    }
    if (!this.isFormFieldFocused || !this.pdfFormSaveEnabled_) {
      return Promise.resolve(false);
    }
    this.waitForFormFocusChange_ = new PromiseResolver();
    return this.waitForFormFocusChange_.promise;
  },

  /** @private */
  onFormFieldFocusedChanged_() {
    if (!this.waitForFormFocusChange_) {
      return;
    }

    this.waitForFormFocusChange_.resolve(this.hasEdits);
    this.waitForFormFocusChange_ = null;
  },

  /** @private */
  onDownloadOriginalClick_() {
    this.fire('save', SaveRequestType.ORIGINAL);
    this.$.downloadMenu.close();
  },

  /** @private */
  onDownloadEditedClick_() {
    this.fire(
        'save',
        this.hasEnteredAnnotationMode ? SaveRequestType.ANNOTATION :
                                        SaveRequestType.EDITED);
    this.$.downloadMenu.close();
  },

  print() {
    this.fire('print');
  },

  undo() {
    this.fire('undo');
  },

  redo() {
    this.fire('redo');
  },

  toggleAnnotation() {
    this.annotationMode = !this.annotationMode;
    if (this.annotationMode) {
      // Select pen tool when entering annotation mode.
      this.updateAnnotationTool_(/** @type {!HTMLElement} */ (this.$.pen));
    }
    this.dispatchEvent(new CustomEvent('annotation-mode-toggled', {
      detail: {
        value: this.annotationMode,
      },
    }));
  },

  /**
   * @param {!Event} e
   * @private
   */
  annotationToolClicked_(e) {
    this.updateAnnotationTool_(/** @type {!HTMLElement} */ (e.currentTarget));
  },

  /**
   * @param {!Event} e
   * @private
   */
  annotationToolOptionChanged_(e) {
    const element = e.currentTarget.parentElement;
    if (!this.annotationTool || element.id !== this.annotationTool.tool) {
      return;
    }
    this.updateAnnotationTool_(e.currentTarget.parentElement);
  },

  /**
   * @param {!HTMLElement} element
   * @private
   */
  updateAnnotationTool_(element) {
    const tool = element.id;
    const options = element.querySelector('viewer-pen-options') || {
      selectedSize: 1,
      selectedColor: undefined,
    };
    const attributeStyleMap = element.attributeStyleMap;
    attributeStyleMap.set('--pen-tip-fill', options.selectedColor);
    attributeStyleMap.set(
        '--pen-tip-border',
        options.selectedColor === '#000000' ? 'currentcolor' :
                                              options.selectedColor);
    this.annotationTool = {
      tool: tool,
      size: options.selectedSize,
      color: options.selectedColor,
    };
  },

  /**
   * @param {string} toolName
   * @return {boolean} Whether the annotation tool is using tool |toolName|.
   * @private
   */
  isAnnotationTool_(toolName) {
    return !!this.annotationTool && this.annotationTool.tool === toolName;
  },

  /**
   * @return {boolean}
   * @private
   */
  hasEditsToSave_() {
    return this.hasEnteredAnnotationMode ||
        (this.pdfFormSaveEnabled_ && this.hasEdits);
  },

  /**
   * @return {string} The value for the aria-haspopup attribute for the download
   *     button.
   * @private
   */
  computeDownloadHasPopup_() {
    return this.hasEditsToSave_() ? 'menu' : 'false';
  },

  /** @private */
  onStringsSet_() {
    assert(this.strings);

    this.pdfAnnotationsEnabled_ =
        loadTimeData.getBoolean('pdfAnnotationsEnabled');
    this.pdfFormSaveEnabled_ = loadTimeData.getBoolean('pdfFormSaveEnabled');
    this.printingEnabled_ = loadTimeData.getBoolean('printingEnabled');
  },
});
