<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div id="filter-items-container">
    <div id="controls">
      <MdButton
          class="md-primary md-raised md-dense"
          @click="checkAll">
        Check All
      </MdButton>
      <MdButton
          class="md-primary md-raised md-dense"
          @click="uncheckAll">
        Uncheck All
      </MdButton>
    </div>
    <MdList
        id="filter-list"
        class="md-scrollbar">
      <MdListItem
          v-for="node in filterList"
          :key="node.name">
        <MdButton
            class="numeric-input-button md-icon-button md-dense"
            @click="removeFromFilter(node.name)">
          <MdIcon>clear</MdIcon>
        </MdButton>
        <MdCheckbox
            v-model="node.checked"
            class="md-primary"/>
        <div class="filter-items-text md-list-item-text">
          {{ shortenName(node.name) }}
        </div>
      </MdListItem>
    </MdList>
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

<style>
#filter-list .md-list-item-content {
  min-height: 0;
  padding: 0;
}
</style>

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

.filter-items-text{
  display: inline-block;
  margin-left: 15px;
  white-space: normal;
  width: 100%;
  word-wrap: break-word;
}

#filter-list {
  max-height: 30vh;
  overflow-y: scroll;
}

#controls {
  display: flex;
  flex-direction: row;
}
</style>
