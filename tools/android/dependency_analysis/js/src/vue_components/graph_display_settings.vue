<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div id="display-settings">
    <div>
      <input
          id="curve-edges"
          v-model="displaySettingsData.curveEdges"
          type="checkbox">
      <label for="curve-edges">Curve graph edges</label>
    </div>
    <div>
      <input
          id="color-on-hover"
          v-model="displaySettingsData.colorOnlyOnHover"
          type="checkbox">
      <label for="color-on-hover">Color graph edges only on node hover</label>
    </div>
    <label for="graph-edge-color">Graph edge color scheme:</label>
    <select
        id="graph-edge-color"
        v-model="displaySettingsData.graphEdgeColor">
      <option
          v-for="edgeColor in GraphEdgeColor"
          :key="edgeColor"
          :value="edgeColor">
        {{ edgeColor }}
      </option>
    </select>
  </div>
</template>

<script>
import {GraphEdgeColor} from '../display_settings_data.js';

// @vue/component
const GraphDisplaySettings = {
  props: {
    displaySettingsData: Object,
  },
  computed: {
    GraphEdgeColor: () => GraphEdgeColor,
  },
};

export default GraphDisplaySettings;
</script>

<style scoped>
#display-settings {
  display: flex;
  flex-direction: column;
  margin-bottom: 10px;
}
</style>
