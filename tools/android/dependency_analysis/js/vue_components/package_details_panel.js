// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {shortenClassName} from '../chrome_hooks.js';
import {PagePathName} from '../url_processor.js';
import {LinkToGraph} from './link_to_graph.js';

const PackageDetailsPanel = {
  components: {
    'link-to-graph': LinkToGraph,
  },
  props: ['selectedPackage'],
  computed: {
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
  template: `
    <div
      v-if="this.selectedPackage !== null"
      class="package-details-panel"
    >
      <link-to-graph
        :filter="this.classesInSelectedPackage"
        graphType=${PagePathName.CLASS}
        text="Class graph with all classes in this package"
      ></link-to-graph>
      <ul>
        <li v-for="classObj in this.classesWithShortNames">
          <link-to-graph
            :filter="[classObj.name]"
            graphType=${PagePathName.CLASS}
            :text="classObj.shortName"
          ></link-to-graph>
        </li>
      </ul>
    </div>`,
};

export {
  PackageDetailsPanel,
};
