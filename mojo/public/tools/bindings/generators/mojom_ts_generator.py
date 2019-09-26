# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates Typescript source files from a mojom.Module."""

import mojom.generate.generator as generator
from mojom.generate.template_expander import UseJinja

class TypescriptStylizer(generator.Stylizer):
  def StylizeModule(self, mojom_namespace):
    return '.'.join(generator.ToCamel(word, lower_initial=True)
                        for word in mojom_namespace.split('.'))

class Generator(generator.Generator):
  def _GetParameters(self, use_es_modules=False):
    return {
      "module": self.module,
      "use_es_modules": use_es_modules,
   }

  @staticmethod
  def GetTemplatePrefix():
    return "ts_templates"

  def GetFilters(self):
    ts_filters = {}
    return ts_filters

  @UseJinja("mojom.tmpl")
  def _GenerateBindings(self):
    return self._GetParameters()

  @UseJinja("mojom.tmpl")
  def _GenerateESModulesBindings(self):
    return self._GetParameters(use_es_modules=True)

  def GenerateFiles(self, args):
    if self.variant:
      raise Exception("Variants not supported in JavaScript bindings.")

    self.module.Stylize(TypescriptStylizer())

    self.Write(self._GenerateBindings(), "%s-lite.ts" % self.module.path)
    self.Write(self._GenerateESModulesBindings(),
               "%s-lite.m.ts" % self.module.path)
