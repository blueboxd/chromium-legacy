# Copyright 2018 The Feed Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

""" This file contains functions to retrieve dependencies."""

load("@rules_jvm_external//:specs.bzl", "maven")

def get_robolectric_dependency(artifact):
    """ Returns a maven.artifact for a Robolectric dependency.

    This is used to ensure the same version of Robolectric is used and that
    any exclusions are applied.

    Args:
      artifact: The name of the Robolectric artifact to load

    Returns:
      a maven.artifact representing the request Robolectric artifact

    """
    return maven.artifact(
        group = "org.robolectric",
        artifact = artifact,
        version = "4.1",
        exclusions = [
            # The protobuf-java artifact is excluded in order to prevent the
            # full java proto runtime from being pulled in.
            "com.google.protobuf:protobuf-java",
        ],
    )
