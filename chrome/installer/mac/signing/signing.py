# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The signing module defines the various binary pieces of the Chrome application
bundle that need to be signed, as well as providing utilities to sign them.
"""

import os.path

from . import commands


def sign_part(paths, config, part):
    """Code signs a part.

    Args:
        paths: A |model.Paths| object.
        conifg: The |model.CodeSignConfig| object.
        part: The |model.CodeSignedProduct| to sign. The product's |path| must
            be in |paths.work|.
    """
    command = ['codesign', '--sign', config.identity]
    if config.notary_user:
        # Assume if the config has notary authentication information that the
        # products will be notarized, which requires a secure timestamp.
        command.append('--timestamp')
    if part.sign_with_identifier:
        command.extend(['--identifier', part.identifier])
    reqs = part.requirements_string(config)
    if reqs:
        command.extend(['--requirements', '=' + reqs])
    if part.options:
        command.extend(['--options', ','.join(part.options)])
    if part.entitlements:
        command.extend(
            ['--entitlements',
             os.path.join(paths.work, part.entitlements)])
    command.append(os.path.join(paths.work, part.path))
    commands.run_command(command)


def verify_part(paths, part):
    """Displays and verifies the code signature of a part.

    Args:
        paths: A |model.Paths| object.
        part: The |model.CodeSignedProduct| to verify. The product's |path|
            must be in |paths.work|.
    """
    verify_options = list(part.verify_options) if part.verify_options else []
    part_path = os.path.join(paths.work, part.path)
    commands.run_command([
        'codesign', '--display', '--verbose=5', '--requirements', '-', part_path
    ])
    commands.run_command(['codesign', '--verify', '--verbose=6'] +
                         verify_options + [part_path])


def validate_app(paths, config, part):
    """Displays and verifies the signature of a CodeSignedProduct.

    Args:
        paths: A |model.Paths| object.
        conifg: The |model.CodeSignConfig| object.
        part: The |model.CodeSignedProduct| for the outer application bundle.
    """
    app_path = os.path.join(paths.work, part.path)
    commands.run_command([
        'codesign', '--display', '--requirements', '-', '--verbose=5', app_path
    ])
    if config.run_spctl_assess:
        commands.run_command(['spctl', '--assess', '-vv', app_path])
