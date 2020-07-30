// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {TutorialLesson} from './tutorial_lesson.js';

/** @enum {string} */
const Curriculum = {
  OOBE: 'oobe',
  NEW_USER: 'new_user',
  EXPERIENCED_USER: 'experienced_user',
  DEVELOPER: 'developer'
};

/**
 * The user’s interaction medium. Influences tutorial content.
 * @enum {string}
 */
const InteractionMedium = {
  KEYBOARD: 'keyboard',
  TOUCH: 'touch',
  BRAILLE: 'braille',
};

/**
 * The various screens within the tutorial.
 * @enum {string}
 */
const Screen = {
  MAIN_MENU: 'main_menu',
  LESSON_MENU: 'lesson_menu',
  LESSON: 'lesson',
};

Polymer({
  is: 'i-tutorial',

  _template: html`{__html_template__}`,

  properties: {
    curriculum: {type: String, observer: 'updateIncludedLessons'},

    medium: {
      type: String,
      value: InteractionMedium.KEYBOARD,
      observer: 'updateIncludedLessons'
    },

    // Bookkeeping variables.

    // Not all lessons are included, some are filtered out based on the chosen
    // medium and curriculum.
    includedLessons: {type: Array},

    // An index into |includedLessons|.
    activeLessonIndex: {type: Number, value: -1},

    activeLessonNum: {type: Number, value: -1},

    numLessons: {type: Number, value: 0},

    activeScreen: {type: String},

    // Labels and text content.

    chooseYourExperience: {
      type: String,
      value: 'Choose your tutorial experience',
    },

    newUser: {type: String, value: 'New user'},

    experiencedUser: {type: String, value: 'Experienced user'},

    developer: {type: String, value: 'Developer'},

    continue: {type: String, value: 'Continue where I left off'},

    previousLesson: {type: String, value: 'Previous lesson'},

    nextLesson: {type: String, value: 'Next lesson'},

    mainMenu: {type: String, value: 'Main menu'},

    lessonMenu: {type: String, value: 'All lessons'},

    exitTutorial: {type: String, value: 'Exit tutorial'},

    lessonData: {
      type: Array,
      value: [
        {
          content:
              ['Welcome to the ChromeVox tutorial. We noticed this might be ' +
               'your first time using ChromeVox, so let\'s quickly cover the ' +
               `basics. When you're ready, use the space bar to move to the ` +
               'next lesson.'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.OOBE],
          actions: [
            {type: 'key_sequence', value: {keys: {keyCode: [32 /* Space */]}}}
          ],
          autoInteractive: true,
        },

        {
          content: [
            `Let's learn how to navigate the tutorial first. ` +
                'In ChromeVox, the Search key is the modifier key. ' +
                'Most ChromeVox shortcuts start with the Search key. ' +
                'On the Chromebook, the Search key is immediately above the ' +
                `left Shift key. When you're ready, try finding and ` +
                'pressing the search key on your keyboard',
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.OOBE],
          actions: [{
            type: 'key_sequence',
            value: {skipStripping: false, keys: {keyCode: [91 /* Search*/]}},
            afterActionMsg: 'You found the search key!',
          }],
          autoInteractive: true,
        },

        {
          content: [
            `Now that you know where the search key is, let's learn ` +
                'some basic navigation. While holding search, use the arrow ' +
                'keys to move ChromeVox around the screen. Press Search + ' +
                'right arrow to move to the next instruction',
            'You can use Search + right arrow and search + left arrow to ' +
                'navigate the tutorial. Now press Search + right arrow again.',
            'If you reach an item you want to click, press Search + Space. ' +
                'Try doing so now to move to the next lesson',
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.OOBE],
          actions: [
            {
              type: 'key_sequence',
              value: {cvoxModifier: true, keys: {keyCode: [39]}},
              afterActionMsg: 'Great! You pressed Search + right arrow.'
            },
            {
              type: 'key_sequence',
              value: {cvoxModifier: true, keys: {keyCode: [39]}},
            },
            {
              type: 'key_sequence',
              value: {cvoxModifier: true, keys: {keyCode: [32]}},
              afterActionMsg: 'Great! You pressed Search + space',
            }
          ],
          autoInteractive: true,
        },

        {
          title: 'Basic orientation complete!',
          content: [
            'Well done! You have learned the basics of ChromeVox. You can ' +
                `now continue browsing the tutorial with what you've ` +
                'learned, or exit by finding and pressing the Quit Tutorial ' +
                'button',
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.OOBE],
        },

        {
          title: 'On, Off, and Stop',
          content: [
            'To temporarily stop ChromeVox from speaking, press the Control ' +
                'key.',
            'To turn ChromeVox on or off, use Control+Alt+Z.',
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.NEW_USER],
        },

        {
          title: 'The ChromeVox Modifier Key',
          content: [
            'In ChromeVox, the Search key is the modifier key. ' +
                'Most ChromeVox shortcuts start with the Search key. ' +
                'You’ll also use the arrow keys for navigation.',
            'On the Chromebook, the Search key is immediately above the ' +
                'left Shift key.',
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.NEW_USER],
        },

        {
          title: 'Basic Navigation',
          content: [
            'To move forward between items on a page, press Search + Right ' +
                'Arrow, or Search + Left Arrow to jump back.',
            'To go to the next line, press Search + Down Arrow. ' +
                'To get to the previous line, use Search + Up Arrow.',
            'If you reach an item you want to click, press Search + Space.',
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.NEW_USER],
          practiceTitle: 'Basic Navigation Practice',
          practiceInstructions:
              'Try using basic navigation to navigate through the items ' +
              'below. Find the button titled "Click me" and use Search ' +
              '+ Space to click it. Then move to the next lesson.',
          practiceFile: 'basic_navigation',
          practiceState: {
            goal: {click: false},
          },
          events: ['click'],
          hints: [
            'Try pressing Search + left/right arrow. The search key directly ' +
                ' above the shift key',
            'Use search + space to click an item'
          ],
        },

        {
          title: 'Jump Commands',
          content: [
            'Use jump commands to skip to specific types of elements.',
            'To jump forward between headings, press Search + H, or to ' +
                'jump backward, press Search + Shift + H.',
            'To jump forward between buttons, press Search + B, or to ' +
                'jump backward, press Search + Shift + B',
            'To jump foorward beetween links, press Search + L, or to ' +
                'jump backward, press Search + Shift + L'
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.NEW_USER],
          practiceTitle: 'Jump Commands Practice',
          practiceInstructions:
              'Try using what you have learned to navigate by element type. ' +
              'Notice that navigation wraps if you are on the first or ' +
              'last element and press previous element or next element, ' +
              'respectively.',
          practiceFile: 'jump_commands',
          practiceState: {
            'first-heading': {focus: false},
            'first-link': {focus: false},
            'first-button': {focus: false},
            'second-heading': {focus: false},
            'second-link': {focus: false},
            'second-button': {focus: false},
            'last-heading': {focus: false},
            'last-link': {focus: false},
            'last-button': {focus: false},
          },
          events: ['focus'],
          hints: [
            'Try using search + h to move by header',
            'Try using search + b to move by button',
            'Try using search + l to move by link'
          ],
        },

        {
          title: 'The ChromeVox Menu',
          content: [
            'To explore all ChromeVox commands and shortcuts, press ' +
                'Search + Period, then use the Arrow keys to navigate the ' +
                'menus, and Enter to activate a command. Return here by ' +
                'pressing Search+o then t.',
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.NEW_USER]
        },

        {
          title: 'Helpful Chrome Shortcuts',
          content: [
            'The next few shortcuts aren’t ChromeVox commands, but they are ' +
                'still very useful for getting the most out of Chrome.',
            'To navigate forward through actionable items like buttons and ' +
                'links, press the Tab key. To navigate backwards, press ' +
                'Shift+Tab.',
            'To enter the Chrome browser address box, also called the ' +
                'omnibox, press Control + L.',
            'To open and go to a new tab automatically, press Control+T. ' +
                'Your cursor will be in the omnibox.',
            ' To close a tab, press Control+W.',
            'To move forward between open tabs, use Control+Tab.',
            'To open the Chrome browser menu, press Alt+F.',
            'To open the full list of keyboard shortcuts, press ' +
                'Control + Alt + /'
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.NEW_USER]
        },

        {
          title: 'Sounds',
          content: [
            'ChromeVox uses sounds to give you essential and additional ' +
                'information. You can use these sounds to navigate more ' +
                'quickly by learning what each sound means. Once you get ' +
                'more comfortable, you can turn off verbose descriptions in ' +
                'speech and rely on them for essential information about the ' +
                'page. Here is a complete list of sounds and what they mean',
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.NEW_USER]
        },

        {
          title: 'Text fields',
          content: ['Text content for text fields lesson'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.EXPERIENCED_USER],
          practiceTitle: 'Edit fields practice',
          practiceInstructions:
              'Try using what you have learned about text fields and edit ' +
              'the text fields below.',
          practiceFile: 'text_fields',
          practiceState: {
            input: {focus: false, input: false},
            editable: {focus: false, input: false}
          },
          events: ['focus', 'input'],
          hints: [
            'Once you find an editable element, you can type normally.',
            'Try editing the content you entered'
          ]
        },

        {
          title: 'Congratulations',
          content: [
            'You’ve learned the essentials to use ChromeVox successfully.  ' +
                'Remember that you can open the ChromeVox command menu at ' +
                'any time by pressing Search+Period. To learn even more ' +
                'about ChromeVox and Chrome OS, visit the following articles.',
            'If you are done with the tutorial, use ChromeVox to navigate ' +
                'to the Quit button and click it.'
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [
            Curriculum.OOBE, Curriculum.NEW_USER, Curriculum.DEVELOPER,
            Curriculum.EXPERIENCED_USER
          ],
        }
      ]
    }
  },

  /** @override */
  ready() {
    this.showMainMenu();
  },

  /**
   * @param {!MouseEvent} evt
   * @private
   */
  chooseCurriculum(evt) {
    const id = evt.target.id;
    if (id === 'newUserButton') {
      this.curriculum = Curriculum.NEW_USER;
    } else if (id === 'experiencedUserButton') {
      this.curriculum = Curriculum.EXPERIENCED_USER;
    } else if (id === 'developerButton') {
      this.curriculum = Curriculum.DEVELOPER;
    } else if (id === 'oobeButton') {
      this.curriculum = Curriculum.OOBE;
    } else {
      throw new Error('Invalid target for chooseCurriculum: ' + evt.target.id);
    }
    this.showLessonMenu();
  },

  /** @private */
  showNextLesson() {
    this.showLesson(this.activeLessonIndex + 1);
  },

  /** @private */
  showPreviousLesson() {
    this.showLesson(this.activeLessonIndex - 1);
  },

  /**
   * @param {number} index
   * @private
   */
  showLesson(index) {
    this.showLessonContainer();
    if (index < 0 || index >= this.numLessons) {
      return;
    }

    this.activeLessonIndex = index;

    // Lessons observe activeLessonNum. When updated, lessons automatically
    // update their visibility.
    this.activeLessonNum = this.includedLessons[index].lessonNum;

    const lesson = this.includedLessons[this.activeLessonIndex];
    if (lesson.autoInteractive) {
      this.startInteractiveMode(lesson.actions);
    }
  },


  // Methods for hiding and showing screens.

  /** @private */
  showMainMenu() {
    this.activeScreen = Screen.MAIN_MENU;
    this.$.mainMenuHeader.focus();
  },

  /** @private */
  showLessonMenu() {
    this.activeScreen = Screen.LESSON_MENU;
    this.createLessonShortcuts();
    this.$.lessonMenuHeader.focus();
  },

  /** @private */
  showLessonContainer() {
    this.activeScreen = Screen.LESSON;
  },

  /** @private */
  updateIncludedLessons() {
    this.includedLessons = [];
    this.activeLessonNum = -1;
    this.activeLessonIndex = -1;
    this.numLessons = 0;
    const lessons = this.$.lessonContainer.children;
    for (let i = 0; i < lessons.length; ++i) {
      const element = lessons[i];
      if (element.is !== 'tutorial-lesson') {
        continue;
      }

      const lesson = element;
      if (lesson.shouldInclude(this.medium, this.curriculum)) {
        this.includedLessons.push(lesson);
      }
    }
    this.numLessons = this.includedLessons.length;
    this.createLessonShortcuts();
  },

  /** @private */
  createLessonShortcuts() {
    // Clear previous lesson shortcuts, as the user may have chosen a new
    // curriculum or medium for the tutorial.
    this.$.lessonShortcuts.innerHTML = '';

    // Create shortcuts for each included lesson.
    let count = 1;
    for (const lesson of this.includedLessons) {
      const button = document.createElement('cr-button');
      button.addEventListener('click', this.showLesson.bind(this, count - 1));
      button.textContent = lesson.title;
      this.$.lessonShortcuts.appendChild(button);
      count += 1;
    }
  },


  // Methods for computing attributes and properties.

  /**
   * @param {number} activeLessonIndex
   * @param {Screen} activeScreen
   * @return {boolean}
   * @private
   */
  shouldHideNextLessonButton(activeLessonIndex, activeScreen) {
    if (activeLessonIndex === this.numLessons - 1 ||
        activeScreen !== Screen.LESSON) {
      return true;
    }

    return false;
  },

  /**
   * @param {number} activeLessonIndex
   * @param {Screen} activeScreen
   * @return {boolean}
   * @private
   */
  shouldHidePreviousLessonButton(activeLessonIndex, activeScreen) {
    if (activeLessonIndex === 0 || activeScreen !== Screen.LESSON) {
      return true;
    }

    return false;
  },

  /**
   * @param {Screen} activeScreen
   * @return {boolean}
   * @private
   */
  shouldHideMainMenu(activeScreen) {
    if (activeScreen === Screen.MAIN_MENU) {
      return false;
    }

    return true;
  },

  /**
   * @param {Screen} activeScreen
   * @return {boolean}
   * @private
   */
  shouldHideLessonContainer(activeScreen) {
    if (activeScreen === Screen.LESSON) {
      return false;
    }

    return true;
  },

  /**
   * @param {Screen} activeScreen
   * @return {boolean}
   * @private
   */
  shouldHideLessonMenu(activeScreen) {
    if (activeScreen === Screen.LESSON_MENU) {
      return false;
    }

    return true;
  },

  /**
   * @param {Curriculum} curriculum
   * @param {InteractionMedium} medium
   * @return {string}
   * @private
   */
  computeLessonMenuHeader(curriculum, medium) {
    return 'Lessons for the ' + curriculum + ' ' + medium + ' experience';
  },

  /** @private */
  exit() {
    this.dispatchEvent(new CustomEvent('closetutorial', {}));
  },

  // Interactive mode.

  startInteractiveMode(actions) {
    this.dispatchEvent(new CustomEvent(
        'startinteractivemode', {composed: true, detail: {actions}}));
  },
});
