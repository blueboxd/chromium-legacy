# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from . import model, modification, test_common, test_config

mock = test_common.import_mock()


def plist_read(*args):
    bundle_id = test_config.TestConfig().base_bundle_id
    plists = {
        '$W/App Product.app/Contents/Info.plist': {
            'CFBundleIdentifier': bundle_id,
            'KSProductID': 'test.ksproduct',
            'KSChannelID': 'stable',
            'KSChannelID-full': 'stable',
        },
        '$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework/XPCServices/AlertNotificationService.xpc/Contents/Info.plist':
            {
                'CFBundleIdentifier':
                    bundle_id + '.AlertNotificationService.xpc'
            },
        '$W/app-entitlements.plist': {
            'com.apple.application-identifier': bundle_id
        },
        '$W/helper-renderer-entitlements.plist': {},
        '$W/helper-plugin-entitlements.plist': {},
        '$W/App Product Canary.app/Contents/Resources/test.signing.bundle_id.canary.manifest/Contents/Resources/test.signing.bundle_id.canary.manifest':
            {
                'pfm_domain': bundle_id
            }
    }
    plists['$W/App Product Canary.app/Contents/Info.plist'] = plists[
        '$W/App Product.app/Contents/Info.plist']
    return plists[args[0]]


@mock.patch('signing.commands.plistlib',
            **{'readPlist.side_effect': plist_read})
@mock.patch.multiple(
    'signing.commands', **{
        m: mock.DEFAULT for m in ('copy_files', 'move_file', 'make_dir',
                                  'write_file', 'run_command')
    })
class TestModification(unittest.TestCase):

    def setUp(self):
        self.paths = model.Paths('$I', '$O', '$W')
        self.config = test_config.TestConfig()

    def test_base_distribution(self, plistlib, **kwargs):
        dist = model.Distribution()
        config = dist.to_config(self.config)

        modification.customize_distribution(self.paths, dist, config)

        self.assertEqual(1, plistlib.writePlist.call_count)
        plistlib.writePlist.assert_called_with(
            {
                'CFBundleIdentifier': config.base_bundle_id,
                'KSProductID': 'test.ksproduct'
            },
            '$W/App Product.app/Contents/Info.plist',
        )

        self.assertEqual(3, kwargs['copy_files'].call_count)
        kwargs['copy_files'].assert_has_calls([
            mock.call('$I/Product Packaging/app-entitlements.plist',
                      '$W/app-entitlements.plist'),
            mock.call('$I/Product Packaging/helper-renderer-entitlements.plist',
                      '$W/helper-renderer-entitlements.plist'),
            mock.call('$I/Product Packaging/helper-plugin-entitlements.plist',
                      '$W/helper-plugin-entitlements.plist')
        ])
        self.assertEqual(0, kwargs['move_file'].call_count)
        self.assertEqual(0, kwargs['write_file'].call_count)

    def test_distribution_with_brand(self, plistlib, **kwargs):
        dist = model.Distribution(branding_code='MOO')
        config = dist.to_config(self.config)

        modification.customize_distribution(self.paths, dist, config)

        self.assertEqual(1, plistlib.writePlist.call_count)
        plistlib.writePlist.assert_called_with(
            {
                'CFBundleIdentifier': config.base_bundle_id,
                'KSProductID': 'test.ksproduct',
                'KSBrandID': 'MOO'
            },
            '$W/App Product.app/Contents/Info.plist',
        )

        self.assertEqual(3, kwargs['copy_files'].call_count)
        kwargs['copy_files'].assert_has_calls([
            mock.call('$I/Product Packaging/app-entitlements.plist',
                      '$W/app-entitlements.plist'),
            mock.call('$I/Product Packaging/helper-renderer-entitlements.plist',
                      '$W/helper-renderer-entitlements.plist'),
            mock.call('$I/Product Packaging/helper-plugin-entitlements.plist',
                      '$W/helper-plugin-entitlements.plist')
        ])
        self.assertEqual(0, kwargs['move_file'].call_count)

    def test_distribution_with_channel(self, plistlib, **kwargs):
        dist = model.Distribution(channel='dev')
        config = dist.to_config(self.config)

        modification.customize_distribution(self.paths, dist, config)

        self.assertEqual(1, plistlib.writePlist.call_count)
        plistlib.writePlist.assert_called_with(
            {
                'CFBundleIdentifier': config.base_bundle_id,
                'KSProductID': 'test.ksproduct',
                'KSChannelID': 'dev',
                'KSChannelID-full': 'dev-full'
            },
            '$W/App Product.app/Contents/Info.plist',
        )

        self.assertEqual(3, kwargs['copy_files'].call_count)
        kwargs['copy_files'].assert_has_calls([
            mock.call('$I/Product Packaging/app-entitlements.plist',
                      '$W/app-entitlements.plist'),
            mock.call('$I/Product Packaging/helper-renderer-entitlements.plist',
                      '$W/helper-renderer-entitlements.plist'),
            mock.call('$I/Product Packaging/helper-plugin-entitlements.plist',
                      '$W/helper-plugin-entitlements.plist')
        ])
        self.assertEqual(0, kwargs['move_file'].call_count)
        self.assertEqual(0, kwargs['write_file'].call_count)

    def test_distribution_with_product_dirname(self, plistlib, **kwargs):
        dist = model.Distribution(product_dirname='Farmland/Cows')
        config = dist.to_config(self.config)

        modification.customize_distribution(self.paths, dist, config)

        self.assertEqual(1, plistlib.writePlist.call_count)
        plistlib.writePlist.assert_called_with(
            {
                'CFBundleIdentifier': config.base_bundle_id,
                'KSProductID': 'test.ksproduct',
                'CrProductDirName': 'Farmland/Cows'
            },
            '$W/App Product.app/Contents/Info.plist',
        )

        self.assertEqual(3, kwargs['copy_files'].call_count)
        kwargs['copy_files'].assert_has_calls([
            mock.call('$I/Product Packaging/app-entitlements.plist',
                      '$W/app-entitlements.plist'),
            mock.call('$I/Product Packaging/helper-renderer-entitlements.plist',
                      '$W/helper-renderer-entitlements.plist'),
            mock.call('$I/Product Packaging/helper-plugin-entitlements.plist',
                      '$W/helper-plugin-entitlements.plist')
        ])
        self.assertEqual(0, kwargs['move_file'].call_count)
        self.assertEqual(0, kwargs['write_file'].call_count)

    def test_distribution_with_creator_code(self, plistlib, **kwargs):
        dist = model.Distribution(creator_code='Mooo')
        config = dist.to_config(self.config)

        modification.customize_distribution(self.paths, dist, config)

        self.assertEqual(1, plistlib.writePlist.call_count)
        plistlib.writePlist.assert_called_with(
            {
                'CFBundleIdentifier': config.base_bundle_id,
                'KSProductID': 'test.ksproduct',
                'CFBundleSignature': 'Mooo'
            },
            '$W/App Product.app/Contents/Info.plist',
        )

        self.assertEqual(3, kwargs['copy_files'].call_count)
        kwargs['copy_files'].assert_has_calls([
            mock.call('$I/Product Packaging/app-entitlements.plist',
                      '$W/app-entitlements.plist'),
            mock.call('$I/Product Packaging/helper-renderer-entitlements.plist',
                      '$W/helper-renderer-entitlements.plist'),
            mock.call('$I/Product Packaging/helper-plugin-entitlements.plist',
                      '$W/helper-plugin-entitlements.plist')
        ])
        kwargs['write_file'].assert_called_once_with(
            '$W/App Product.app/Contents/PkgInfo', 'APPLMooo')
        self.assertEqual(0, kwargs['move_file'].call_count)

    def test_distribution_with_brand_and_channel(self, plistlib, **kwargs):
        dist = model.Distribution(channel='beta', branding_code='RAWR')
        config = dist.to_config(self.config)

        modification.customize_distribution(self.paths, dist, config)

        self.assertEqual(1, plistlib.writePlist.call_count)
        plistlib.writePlist.assert_called_with(
            {
                'CFBundleIdentifier': config.base_bundle_id,
                'KSProductID': 'test.ksproduct',
                'KSChannelID': 'beta',
                'KSChannelID-full': 'beta-full',
                'KSBrandID': 'RAWR'
            },
            '$W/App Product.app/Contents/Info.plist',
        )

        self.assertEqual(3, kwargs['copy_files'].call_count)
        kwargs['copy_files'].assert_has_calls([
            mock.call('$I/Product Packaging/app-entitlements.plist',
                      '$W/app-entitlements.plist'),
            mock.call('$I/Product Packaging/helper-renderer-entitlements.plist',
                      '$W/helper-renderer-entitlements.plist'),
            mock.call('$I/Product Packaging/helper-plugin-entitlements.plist',
                      '$W/helper-plugin-entitlements.plist')
        ])
        self.assertEqual(0, kwargs['move_file'].call_count)
        self.assertEqual(0, kwargs['write_file'].call_count)

    def test_customize_channel(self, plistlib, **kwargs):
        dist = model.Distribution(
            channel='canary',
            app_name_fragment='Canary',
            product_dirname='Acme/Product Canary',
            creator_code='Mooo',
            channel_customize=True)
        config = dist.to_config(self.config)

        modification.customize_distribution(self.paths, dist, config)

        # Order of file moves is significant.
        self.assertEqual(kwargs['move_file'].mock_calls, [
            mock.call('$W/App Product.app', '$W/App Product Canary.app'),
            mock.call(
                '$W/App Product Canary.app/Contents/MacOS/App Product',
                '$W/App Product Canary.app/Contents/MacOS/App Product Canary'),
            mock.call(
                '$W/App Product Canary.app/Contents/Resources/test.signing.bundle_id.manifest/Contents/Resources/test.signing.bundle_id.manifest',
                '$W/App Product Canary.app/Contents/Resources/test.signing.bundle_id.manifest/Contents/Resources/test.signing.bundle_id.canary.manifest'
            ),
            mock.call(
                '$W/App Product Canary.app/Contents/Resources/test.signing.bundle_id.manifest',
                '$W/App Product Canary.app/Contents/Resources/test.signing.bundle_id.canary.manifest'
            ),
        ])

        self.assertEqual(5, kwargs['copy_files'].call_count)
        kwargs['copy_files'].assert_has_calls([
            mock.call('$I/Product Packaging/app-entitlements.plist',
                      '$W/app-entitlements.plist'),
            mock.call('$I/Product Packaging/helper-renderer-entitlements.plist',
                      '$W/helper-renderer-entitlements.plist'),
            mock.call('$I/Product Packaging/helper-plugin-entitlements.plist',
                      '$W/helper-plugin-entitlements.plist'),
            mock.call('$I/Product Packaging/app_canary.icns',
                      '$W/App Product Canary.app/Contents/Resources/app.icns'),
            mock.call(
                '$I/Product Packaging/document_canary.icns',
                '$W/App Product Canary.app/Contents/Resources/document.icns')
        ])
        kwargs['write_file'].assert_called_once_with(
            '$W/App Product Canary.app/Contents/PkgInfo', 'APPLMooo')

        self.assertEqual(6, plistlib.writePlist.call_count)
        plistlib.writePlist.assert_has_calls([
            mock.call({
                'CFBundleIdentifier':
                    'test.signing.bundle_id.canary.AlertNotificationService.xpc'
            }, '$W/App Product Canary.app/Contents/Frameworks/Product Framework.framework/XPCServices/AlertNotificationService.xpc/Contents/Info.plist'
                     ),
            mock.call({
                'CFBundleIdentifier': config.base_bundle_id,
                'CFBundleExecutable': config.app_product,
                'KSProductID': 'test.ksproduct.canary',
                'KSChannelID': 'canary',
                'KSChannelID-full': 'canary-full',
                'CrProductDirName': 'Acme/Product Canary',
                'CFBundleSignature': 'Mooo'
            }, '$W/App Product Canary.app/Contents/Info.plist'),
            mock.call({
                'com.apple.application-identifier':
                    'test.signing.bundle_id.canary'
            }, '$W/app-entitlements.plist'),
            mock.call({}, '$W/helper-renderer-entitlements.plist'),
            mock.call({}, '$W/helper-plugin-entitlements.plist'),
            mock.call({
                'pfm_domain': 'test.signing.bundle_id.canary'
            }, '$W/App Product Canary.app/Contents/Resources/test.signing.bundle_id.canary.manifest/Contents/Resources/test.signing.bundle_id.canary.manifest'
                     )
        ])
