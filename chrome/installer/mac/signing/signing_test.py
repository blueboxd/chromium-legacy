# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from . import model, signing, test_common, test_config

mock = test_common.import_mock()

# python2 support.
try:
    FileNotFoundError
except NameError:
    FileNotFoundError = IOError


@mock.patch('signing.commands.run_command')
class TestSignPart(unittest.TestCase):

    def setUp(self):
        self.paths = model.Paths('$I', '$O', '$W')
        self.config = test_config.TestConfig()

    def test_sign_part(self, run_command):
        part = model.CodeSignedProduct('Test.app', 'test.signing.app')
        signing.sign_part(self.paths, self.config, part)
        run_command.assert_called_once_with([
            'codesign', '--sign', '[IDENTITY]', '--timestamp', '--requirements',
            '=designated => identifier "test.signing.app"', '$W/Test.app'
        ])

    def test_sign_part_no_notary(self, run_command):
        config = test_config.TestConfig(notary_user=None, notary_password=None)
        part = model.CodeSignedProduct('Test.app', 'test.signing.app')
        signing.sign_part(self.paths, config, part)
        run_command.assert_called_once_with([
            'codesign', '--sign', '[IDENTITY]', '--requirements',
            '=designated => identifier "test.signing.app"', '$W/Test.app'
        ])

    def test_sign_part_no_identifier_requirement(self, run_command):
        part = model.CodeSignedProduct(
            'Test.app', 'test.signing.app', identifier_requirement=False)
        signing.sign_part(self.paths, self.config, part)
        run_command.assert_called_once_with(
            ['codesign', '--sign', '[IDENTITY]', '--timestamp', '$W/Test.app'])

    def test_sign_with_identifier(self, run_command):
        part = model.CodeSignedProduct(
            'Test.app', 'test.signing.app', sign_with_identifier=True)
        signing.sign_part(self.paths, self.config, part)
        run_command.assert_called_once_with([
            'codesign', '--sign', '[IDENTITY]', '--timestamp', '--identifier',
            'test.signing.app', '--requirements',
            '=designated => identifier "test.signing.app"', '$W/Test.app'
        ])

    def test_sign_with_identifier_no_requirement(self, run_command):
        part = model.CodeSignedProduct(
            'Test.app',
            'test.signing.app',
            sign_with_identifier=True,
            identifier_requirement=False)
        signing.sign_part(self.paths, self.config, part)
        run_command.assert_called_once_with([
            'codesign', '--sign', '[IDENTITY]', '--timestamp', '--identifier',
            'test.signing.app', '$W/Test.app'
        ])

    def test_sign_part_with_options(self, run_command):
        part = model.CodeSignedProduct(
            'Test.app',
            'test.signing.app',
            options=model.CodeSignOptions.RESTRICT +
            model.CodeSignOptions.LIBRARY_VALIDATION)
        signing.sign_part(self.paths, self.config, part)
        run_command.assert_called_once_with([
            'codesign', '--sign', '[IDENTITY]', '--timestamp', '--requirements',
            '=designated => identifier "test.signing.app"', '--options',
            'restrict,library', '$W/Test.app'
        ])

    def test_sign_part_with_entitlements(self, run_command):
        part = model.CodeSignedProduct(
            'Test.app',
            'test.signing.app',
            entitlements='entitlements.plist',
            identifier_requirement=False)
        signing.sign_part(self.paths, self.config, part)
        run_command.assert_called_once_with([
            'codesign', '--sign', '[IDENTITY]', '--timestamp', '--entitlements',
            '$W/entitlements.plist', '$W/Test.app'
        ])

    def test_verify_part(self, run_command):
        part = model.CodeSignedProduct('Test.app', 'test.signing.app')
        signing.verify_part(self.paths, part)
        self.assertEqual(run_command.mock_calls, [
            mock.call([
                'codesign', '--display', '--verbose=5', '--requirements', '-',
                '$W/Test.app'
            ]),
            mock.call(['codesign', '--verify', '--verbose=6', '$W/Test.app']),
        ])

    def test_verify_part_with_options(self, run_command):
        part = model.CodeSignedProduct(
            'Test.app',
            'test.signing.app',
            verify_options=model.VerifyOptions.DEEP +
            model.VerifyOptions.IGNORE_RESOURCES)
        signing.verify_part(self.paths, part)
        self.assertEqual(run_command.mock_calls, [
            mock.call([
                'codesign', '--display', '--verbose=5', '--requirements', '-',
                '$W/Test.app'
            ]),
            mock.call([
                'codesign', '--verify', '--verbose=6', '--deep',
                '--ignore-resources', '$W/Test.app'
            ]),
        ])
