#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Unit test suite that runs all Python based docserver tests.'''

import os.path
import sys

_SERVER_DIR = os.path.dirname(__file__)
_SRC_DIR = os.path.join(_SERVER_DIR, *((os.pardir, ) * 5))
_TYP_DIR = os.path.join(_SRC_DIR, 'third_party', 'catapult', 'third_party',
                        'typ')

if not _TYP_DIR in sys.path:
  sys.path.append(_TYP_DIR)

import typ

def resolve(*paths):
  return [os.path.join(_SERVER_DIR, *(p.split('/'))) for p in paths]


# Run build_server so that files needed by tests are copied to the local
# third_party directory.
import build_server
build_server.main()

sys.exit(
  typ.main(tests=resolve(
    'api_categorizer_test.py',
    'api_data_source_test.py',
    'api_list_data_source_test.py',
    'api_models_test.py',
    'api_schema_graph_test.py',
    'app_yaml_helper_test.py',
    'availability_finder_test.py',
    'branch_utility_test.py',
    'cache_chain_object_store_test.py',
    'caching_file_system_test.py',
    'caching_rietveld_patcher_test.py',
    'chained_compiled_file_system_test.py',
    'chroot_file_system_test.py',
    'compiled_file_system_test.py',
    'content_provider_test.py',
    'content_providers_test.py',
    'directory_zipper_test.py',
    'docs_server_utils_test.py',
    'document_parser_test.py',
    'document_renderer_test.py',
    'environment_test.py',
    'features_bundle_test.py',
    'file_system_test.py',
    'future_test.py',
    'handler_test.py',
    'host_file_system_iterator_test.py',
    'host_file_system_provider_test.py',
    'instance_servlet_test.py',
    'integration_test.py',
    'jsc_view_test.py',
    'link_error_detector_test.py',
    'local_file_system_test.py',
    'manifest_data_source_test.py',
    'manifest_features_test.py',
    'mock_file_system_test.py',
    'mock_function_test.py',
    'object_store_creator_test.py',
    'owners_data_source_test.py',
    'patch_servlet_test.py',
    'patched_file_system_test.py',
    'path_canonicalizer_test.py',
    'path_util_test.py',
    'permissions_data_source_test.py',
    'persistent_object_store_test.py',
    'platform_bundle_test.py',
    'platform_util_test.py',
    'redirector_test.py',
    'reference_resolver_test.py',
    'rietveld_patcher_test.py',
    'samples_model_test.py',
    'schema_processor_test.py',
    'sidenav_data_source_test.py',
    'template_data_source_test.py',
    'template_renderer_test.py',
    'test_file_system_test.py',
    'test_object_store_test.py',
    'test_servlet_test.py',
    'whats_new_data_source_test.py',
    # 'render_servlet_test.py', (This has a test failure currently).
  )))
