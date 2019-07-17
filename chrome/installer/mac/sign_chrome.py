#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os.path
import sys

sys.path.append(os.path.dirname(__file__))

from signing import config, model, pipeline


def create_config(config_args, development):
    """Creates the |model.CodeSignConfig| for the signing operations.

    If |development| is True, the config will be modified to not require
    restricted internal assets, nor will the products be required to match
    specific certificate hashes.

    Args:
        config_args: List of args to expand to the config class's constructor.
        development: Boolean indicating whether or not to modify the chosen
            config for development testing.

    Returns:
        An instance of |model.CodeSignConfig|.
    """
    config_class = config.CodeSignConfig
    try:
        import signing.internal_config
        config_class = signing.internal_config.InternalCodeSignConfig
    except ImportError as e:
        # If the build specified Google Chrome as the product, then the
        # internal config has to be available.
        if config_class(identity, keychain).product == 'Google Chrome':
            raise e

    if development:

        class DevelopmentCodeSignConfig(config_class):

            @property
            def codesign_requirements_basic(self):
                return ''

            @property
            def provisioning_profile_basename(self):
                return None

            @property
            def run_spctl_assess(self):
                return False

        config_class = DevelopmentCodeSignConfig

    return config_class(*config_args)


def main():
    parser = argparse.ArgumentParser(
        description='Code sign and package Chrome for channel distribution.')
    parser.add_argument(
        '--keychain', help='The keychain to load the identity from.')
    parser.add_argument(
        '--identity', required=True, help='The identity to sign with.')
    parser.add_argument(
        '--notary-user',
        help='The username used to authenticate to the Apple notary service.')
    parser.add_argument(
        '--notary-password',
        help='The password or password reference (e.g. @keychain, see '
        '`xcrun altool -h`) used to authenticate to the Apple notary service.')
    parser.add_argument(
        '--notary-asc-provider',
        help='The ASC provider string to be used as the `--asc-provider` '
        'argument to `xcrun altool`, to be used when --notary-user is '
        'associated with multiple Apple developer teams. See `xcrun altool -h. '
        'Run `iTMSTransporter -m provider -account_type itunes_connect -v off '
        '-u USERNAME -p PASSWORD` to list valid providers.')
    parser.add_argument(
        '--development',
        action='store_true',
        help='The specified identity is for development. Certain codesign '
        'requirements will be omitted.')
    parser.add_argument(
        '--input',
        required=True,
        help='Path to the input directory. The input directory should '
        'contain the products to sign, as well as the Packaging directory.')
    parser.add_argument(
        '--output',
        required=True,
        help='Path to the output directory. The signed DMG products and '
        'installer tools will be placed here.')

    group = parser.add_mutually_exclusive_group(required=False)
    group.add_argument(
        '--dmg',
        dest='dmg',
        action='store_true',
        help='Defaults to True. Package the signed application into a DMG, '
        'and sign the result.')
    group.add_argument('--no-dmg', dest='dmg', action='store_false')

    group = parser.add_mutually_exclusive_group(required=False)
    group.add_argument(
        '--notarize',
        dest='notarize',
        action='store_true',
        help='Defaults to False. Submit the signed application and DMG to '
        'Apple for notarization.')
    group.add_argument('--no-notarize', dest='notarize', action='store_false')

    parser.set_defaults(dmg=True, notarize=False)
    args = parser.parse_args()

    if args.notarize:
        if not args.notary_user or not args.notary_password:
            parser.error('The --notary-user and --notary-password arguments '
                         'are required with --notarize.')

    config = create_config((args.identity, args.keychain, args.notary_user,
                            args.notary_password, args.notary_asc_provider),
                           args.development)
    paths = model.Paths(args.input, args.output, None)

    if not os.path.exists(paths.output):
        os.mkdir(paths.output)

    pipeline.sign_all(
        paths, config, package_dmg=args.dmg, do_notarization=args.notarize)


if __name__ == '__main__':
    main()
