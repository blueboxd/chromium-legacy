// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CUSTOM_EVENTS} from '../vue_custom_events.js';

const GraphSelectedNodeDetails = {
  props: ['selectedNodeDetailsData'],
  data: function() {
    return this.selectedNodeDetailsData;
  },
  methods: {
    addSelectedToFilter: function() {
      this.$emit(CUSTOM_EVENTS.ADD_TO_FILTER_CLICKED, this.selectedNode.id);
    },
    removeSelectedFromFilter: function() {
      this.$emit(
          CUSTOM_EVENTS.REMOVE_FROM_FILTER_CLICKED, this.selectedNode.id);
    },
  },
  template: `
    <div class="selected-node-details">
      <template v-if="selectedNode !== null">
        <ul>
          <li>Name: {{selectedNode.id}}</li>
          <li>Display Name: {{selectedNode.displayName}}</li>
          <li v-for="(value, key) in selectedNode.visualizationState">
            {{key}}: {{value}}
          </li>
        </ul>
        <button
          v-if="selectedNode.visualizationState.selectedByFilter"
          @click="removeSelectedFromFilter">
          Remove from filter
        </button>
        <button v-else @click="addSelectedToFilter">Add to filter</button>
      </template>
      <div v-else>Click a node for more details.</div>
    </div>`,
};

export {
  GraphSelectedNodeDetails,
};
