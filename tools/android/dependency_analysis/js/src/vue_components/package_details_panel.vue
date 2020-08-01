<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div
      v-if="selectedPackage !== null"
      class="package-details-panel">
    <LinkToGraph
        :filter="packageClassNames"
        :graph-type="PagePathName.CLASS"
        text="Class graph with all classes in this package"/>
    <ul>
      <li
          v-for="classObj in packageClassObjects"
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

// @vue/component
const PackageDetailsPanel = {
  components: {
    LinkToGraph,
  },
  props: {
    selectedPackage: Object,
  },
  computed: {
    PagePathName: () => PagePathName,
    packageClassNames: function() {
      return this.selectedPackage.classNames;
    },
    packageClassObjects: function() {
      return this.selectedPackage.classNames.map(className => {
        return {
          name: className,
          shortName: shortenClassName(className),
        };
      });
    },
  },
};

export default PackageDetailsPanel;
</script>

<style scoped>
.package-details-panel {
  max-height: 300px;
  overflow: hidden;
  overflow-y: scroll;
}
</style>
