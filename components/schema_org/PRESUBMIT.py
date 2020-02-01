# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Chromium presubmit for components/schema_org/generate_schema_org_code."""


def _RunMakeGenerateSchemaOrgCodeTests(input_api, output_api):
    """Runs tests for generate_schema_org_code if related files were changed."""
    files = ('components/schema_org/generate_schema_org_code.py',
             'components/schema_org/generate_schema_org_code_unittest.py')
    if not any(f in input_api.LocalPaths() for f in files):
        return []
    test_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                       'generate_schema_org_code_unittest.py')
    cmd_name = 'generate_schema_org_code_unittest'
    cmd = [input_api.python_executable, test_path]
    test_cmd = input_api.Command(
        name=cmd_name,
        cmd=cmd,
        kwargs={},
        message=output_api.PresubmitPromptWarning)
    return input_api.RunTests([test_cmd])


def CheckChangeOnUpload(input_api, output_api):
    return _RunMakeGenerateSchemaOrgCodeTests(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _RunMakeGenerateSchemaOrgCodeTests(input_api, output_api)
