<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div
      v-if="selectedPackage !== null"
      class="package-details-panel">
    <LinkToGraph
        :filter="classesInSelectedPackage"
        :graph-type="PagePathName.CLASS"
        text="Class graph with all classes in this package"/>
    <ul>
      <li
          v-for="classObj in classesWithShortNames"
          :key="classObj.name">
        <LinkToGraph
            :filter="[classObj.name]"
            :graph-type="PagePathName.CLASS"
            :text="classObj.shortName"/>
      </li>
    </ul>
  </div>
</template>

<script>
import {PagePathName} from '../url_processor.js';
import {shortenClassName} from '../chrome_hooks.js';

import LinkToGraph from './link_to_graph.vue';

const PackageDetailsPanel = {
  components: {
    LinkToGraph,
  },
  props: {
    selectedPackage: Object,
  },
  computed: {
    PagePathName: () => PagePathName,
    classesInSelectedPackage: function() {
      return this.selectedPackage.classNames;
    },
    classesWithShortNames: function() {
      return this.classesInSelectedPackage.map(className => ({
        name: className,
        shortName: shortenClassName(className),
      }));
    },
  },
};

export default PackageDetailsPanel;
</script>

<style scoped>
.package-details-panel {
  max-height: 400px;
  overflow: hidden;
  overflow-y: scroll;
}
</style>
