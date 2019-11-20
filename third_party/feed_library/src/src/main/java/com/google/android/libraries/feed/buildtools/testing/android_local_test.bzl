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

"""Default values for the android_local_test manifest.

  All android_local_test in the Feed libraries should use these values and
  override within the test itself when necessary.

  targetSdkVersion should be set to the current version of Android in source.

  In case the code under test uses statements like:

    if (Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP) { ...}

  test authors should write a test for both branches and override the targetSdkVersion
  using the method annotation @Config. E.g.:

      @Config(sdk=Build.VERSION_CODES.LOLLIPOP)

  minSdkVersion is the lowest API level that we ship our library to and should be updated when we
      no longer support a certain version of Android.
"""
DEFAULT_ANDROID_LOCAL_TEST_MANIFEST = {"minSdkVersion": "16", "targetSdkVersion": "27"}
