// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export const TutorialLesson = Polymer({
  is: 'tutorial-lesson',

  _template: html`{__html_template__}`,

  properties: {
    lessonNum: {type: Number},

    title: {type: String},

    content: {type: Array},

    medium: {type: String},

    curriculums: {type: Array},

    testAreaTitle: {type: String},

    testAreaInstructions: {type: String},

    testAreaFile: {type: String},

    testAreaState: {type: Object},

    events: {type: Array},

    hints: {type: Array},

    hintCounter: {type: Number, value: 0},

    hintIntervalId: {type: Number},

    goalStateReached: {type: Boolean, value: false},

    // Observed properties.

    activeLessonNum: {type: Number, observer: '_setVisibility'},
  },

  /** @override */
  ready() {
    if (this.testAreaFile) {
      this.populateTestArea_();
      for (const evt of this.events) {
        this.$.testArea.addEventListener(
            evt, this.onTestAreaEvent.bind(this), true);
      }
    }
  },

  /**
   * Updates this lessons visibility whenever the active lesson of the tutorial
   * changes.
   */
  _setVisibility() {
    if (this.lessonNum === this.activeLessonNum) {
      this.show();
    } else {
      this.hide();
    }
  },

  show() {
    this.$.lessonContainer.hidden = false;
    this.$.lessonTitle.focus();
  },

  hide() {
    this.$.lessonContainer.hidden = true;
  },


  // Methods for managing the test area.


  /**
   * Asynchronously populates test area.
   * @private
   */
  populateTestArea_() {
    const path = '../i_tutorial/lessons/' + this.testAreaFile + '.html';
    const xhr = new XMLHttpRequest();
    xhr.open('GET', path, true);
    xhr.onload = (evt) => {
      if (xhr.readyState === 4 && xhr.status === 200) {
        this.$.testArea.innerHTML = xhr.responseText;
      } else {
        console.error(xhr.statusText);
      }
    };
    xhr.onerror = function(evt) {
      console.error('Failed to open test area file: ' + path);
      console.error(xhr.statusText);
    };
    xhr.send(null);
  },

  showTestArea() {
    this.$.testAreaContainer.showModal();
    this.startHints();
  },

  closeTestArea() {
    this.stopHints();
    this.$.showTestArea.focus();
  },


  // Methods for tracking the state of the test area.


  /** @param {Event} event */
  onTestAreaEvent(event) {
    const elt = event.target.id;
    const type = event.type;
    // Maybe update goal state.
    if (elt in this.testAreaState) {
      if (type in this.testAreaState[elt]) {
        this.testAreaState[elt][type] = true;
      }
    }

    if (this.isGoalStateReached()) {
      this.onGoalStateReached();
    }
  },

  /** @return {boolean} */
  isGoalStateReached() {
    if (this.goalStateReached === true) {
      return true;
    }

    for (const [elt, state] of Object.entries(this.testAreaState)) {
      for (const [evt, performed] of Object.entries(state)) {
        if (performed == false) {
          return false;
        }
      }
    }
    return true;
  },

  onGoalStateReached() {
    const previousState = this.goalStateReached;
    this.goalStateReached = true;
    if (previousState === false) {
      // Only perform when crossing the threshold from not reached to reached.
      this.stopHints();
      this.requestSpeech(
          'You have passed this tutorial lesson. Find and press the close ' +
          'test area button to continue');
    }
  },


  // Methods for managing hints.


  startHints() {
    this.hintCounter = 0;
    this.hintIntervalId = setInterval(() => {
      if (this.hintCounter >= this.hints.length) {
        this.stopHints();
        return;
      }
      this.requestSpeech(this.hints[this.hintCounter]);
      this.hintCounter += 1;
    }, 20000);
  },

  stopHints() {
    if (this.hintIntervalId) {
      clearInterval(this.hintIntervalId);
    }
  },


  // Miscellaneous methods.

  /**
   * @param {string} medium
   * @param {string} curriculum
   * @return {boolean}
   */
  shouldInclude(medium, curriculum) {
    if (this.medium === medium && this.curriculums.includes(curriculum)) {
      return true;
    }

    return false;
  },

  /**
   * Requests speech from the Panel.
   * @param {string} text
   */
  requestSpeech(text) {
    this.dispatchEvent(
        new CustomEvent('request-speech', {composed: true, detail: {text}}));
  },

  /** @return {boolean} */
  shouldHideTestAreaButton_() {
    if (!this.testAreaFile) {
      return true;
    }

    return false;
  },
});