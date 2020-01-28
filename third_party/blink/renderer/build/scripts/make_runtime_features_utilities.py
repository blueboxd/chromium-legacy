# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from collections import defaultdict


def _runtime_features_graph_sanity_check(features):
    """
    Raises AssertionError when sanity check failed.
    @param features: a List[Dict]. Each Dict must have key 'depends_on' and 'name'
    @returns None
    """
    feature_pool = {str(f['name']) for f in features}
    for f in features:
        for d in f['depends_on']:
            assert d in feature_pool, "{} not found in runtime_enabled_features.json5".format(d)

    def cyclic(features):
        """
        Returns True if the runtime features graph contains a cycle
        @returns bool
        """
        graph = {str(feature['name']): feature['depends_on'] for feature in features}
        path = set()

        def visit(vertex):
            path.add(vertex)
            for neighbor in graph[vertex]:
                if neighbor in path or visit(neighbor):
                    return True
            path.remove(vertex)
            return False

        return any(visit(str(f['name'])) for f in features)

    assert not cyclic(features), "Cycle found in dependency graph"


def origin_trials(features):
    """
    This function returns all features that are in origin trial.
    The dependency is considered in origin trial if itself is in origin trial
    or any of its dependencies are in origin trial. Propagate dependency
    tag use DFS can find all features that are in origin trial.

    @param features: a List[Dict]. Each Dict much has key named 'depends_on'
    @returns Set[str(runtime feature name)]
    """
    _runtime_features_graph_sanity_check(features)

    origin_trials_set = set()

    graph = defaultdict(list)
    for feature in features:
        for dependency in feature['depends_on']:
            graph[dependency].append(str(feature['name']))

    def dfs(node):
        origin_trials_set.add(node)
        for dependent in graph[node]:
            if dependent not in origin_trials_set:
                dfs(dependent)

    for feature in features:
        if feature['origin_trial_feature_name']:
            dfs(str(feature['name']))

    return origin_trials_set
