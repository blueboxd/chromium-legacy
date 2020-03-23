# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The parts module defines the various binary pieces of the Updater application
bundle that need to be signed, as well as providing a function to sign them.
"""

import os.path

from . import commands, signing
from .model import CodeSignOptions, CodeSignedProduct, VerifyOptions


def get_parts(config):
    """Returns all the |model.CodeSignedProduct| objects to be signed for an
    updater application bundle.

    Args:
        config: The |config.CodeSignConfig|.

    Returns:
        A dictionary of |model.CodeSignedProduct|. The keys are short
        identifiers that have no bearing on the actual signing operations.
    """
    return {
        'app':
        CodeSignedProduct(
            '{.app_product}.app'.format(config),
            config.base_bundle_id,
            options=CodeSignOptions.FULL_HARDENED_RUNTIME_OPTIONS,
            requirements=config.codesign_requirements_outer_app,
            identifier_requirement=False,
            entitlements=None,
            verify_options=VerifyOptions.DEEP + VerifyOptions.STRICT)
    }


def sign_all(paths, config):
    """Code signs the application bundle and all of its internal nested code
    parts.

    Args:
        paths: A |model.Paths| object.
        config: The |model.CodeSignConfig| object. The |app_product| binary and
            nested binaries must exist in |paths.work|.
    """
    parts = get_parts(config)

    for name, part in parts.items():
        if name in ('app'):  # Defer app signing to the last step.
            continue
        signing.sign_part(paths, config, part)

    # Sign the outer app bundle.
    signing.sign_part(paths, config, parts['app'])

    # Verify all the parts.
    for part in parts.values():
        signing.verify_part(paths, part)

    # Display the code signature.
    signing.validate_app(paths, config, parts['app'])
