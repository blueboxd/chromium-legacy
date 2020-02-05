# Performance Manager Overview

The Performance Manager centralizes policy for data-driven resource management
and planning. Central to this is the [graph](graph/graph_impl.h) which is
comprised of nodes that reflect the coarse structure of a browser at the
level of [pages](graph/page_node_impl.h), [frames](graph/frame_node_impl.h)
[processes](graph/process_node_impl.h), [workers](graph/worker_node_impl.h) and so forth.

Each node is adorned with relationships and properties sufficient to allow performance management policy
to reason about such things as resource usage and prioritization. Usually properties are added directly
to nodes if they would be used by multiple clients. Otherwise, it's preferred to 
add them as private impl details via [NodeAttachedData](graph/node_attached_data_impl.h). See
[ProcessPriorityAggregator](/chrome/browser/performance_manager/decorators/process_priority_aggregator.cc) as an example of
such a use-case. It requires some per-process-node intermediate data which is stored as NodeAttachedData.

Nodes are nothing but data storage and contain no logic. Properties that need
computation to be maintained are maintained via [decorators](decorators/) or "aggregators"
which are both observers and mutators of the graph.
