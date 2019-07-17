# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import imp
import os

THIS_DIR = os.path.abspath(os.path.dirname(__file__))

config = imp.load_source('signing.config', os.path.join(THIS_DIR,
                                                        'config.py.in'))


class TestConfig(config.CodeSignConfig):

    def __init__(self,
                 identity='[IDENTITY]',
                 keychain='[KEYCHAIN]',
                 notary_user='[NOTARY-USER]',
                 notary_password='[NOTARY-PASSWORD]',
                 notary_asc_provider=None):
        super(TestConfig, self).__init__(identity, keychain, notary_user,
                                         notary_password, notary_asc_provider)

    @property
    def app_product(self):
        return 'App Product'

    @property
    def product(self):
        return 'Product'

    @property
    def version(self):
        return '99.0.9999.99'

    @property
    def base_bundle_id(self):
        return 'test.signing.bundle_id'

    @property
    def optional_parts(self):
        return set()

    @property
    def provisioning_profile_basename(self):
        return 'provisiontest'

    @property
    def run_spctl_assess(self):
        return True
