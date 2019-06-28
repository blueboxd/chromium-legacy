import {Polymer, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import './some_other_style.m.js';
const styleElement = document.createElement('dom-module');
styleElement.innerHTML = `
  <template>
    <style include="some-other-style">
      :host {
        margin: 0;
      }
    </style>
  </template>
`;
styleElement.register('cr-foo-style');