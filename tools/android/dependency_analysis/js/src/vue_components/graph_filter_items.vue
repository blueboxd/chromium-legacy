<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div id="filter-items-container">
    <div id="controls">
      <button @click="checkAll">
        Check All
      </button>
      <button @click="uncheckAll">
        Uncheck All
      </button>
    </div>
    <ul id="filter-list">
      <li
          v-for="node in filterList"
          :key="node.name">
        <div class="filter-list-item">
          <div @click="removeFromFilter(node.name)">
            x
          </div>
          <input
              v-model="node.checked"
              type="checkbox">
          <div>{{ shortenName(node.name) }}</div>
        </div>
      </li>
    </ul>
  </div>
</template>

<script>
import {CUSTOM_EVENTS} from '../vue_custom_events.js';

// @vue/component
const GraphFilterItems = {
  props: {
    nodeFilterData: Object,
    shortenName: Function,
  },
  data: function() {
    return this.nodeFilterData;
  },
  methods: {
    removeFromFilter: function(nodeName) {
      this.$emit(CUSTOM_EVENTS.FILTER_REMOVE, nodeName);
    },
    checkAll: function() {
      this.$emit(CUSTOM_EVENTS.FILTER_CHECK_ALL);
    },
    uncheckAll: function() {
      this.$emit(CUSTOM_EVENTS.FILTER_UNCHECK_ALL);
    },
  },
};

export default GraphFilterItems;
</script>

<style scoped>
ul {
  list-style-type: none;
}

#filter-items-container {
  display: flex;
  flex-direction: column;
  margin-right: 20px;
  min-width: 100px;
}

#filter-list {
  margin: 0;
  overflow-x: hidden;
  overflow-y: scroll;
  padding: 0;
}

#controls {
  display: flex;
  flex-direction: row;
}

.filter-list-item {
  display: flex;
  flex-direction: row;
}
</style>
