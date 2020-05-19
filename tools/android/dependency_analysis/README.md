# Chrome Android Dependency Analysis Tool
## Overview
As part of Chrome Modularization, this directory contains various tools for
analyzing the dependencies contained within the Chrome Android project.

## Usage
Currently there is only one tool.

### JDeps Parser
Runs jdeps on a JAR file and constructs a graph data structure in-memory of the
dependencies. Currently only creates the graph for class-level dependencies.

usage:
  python3 process\_jdeps.py [-h] filepath
positional arguments:
  filepath: Path of the JAR to run jdeps on

