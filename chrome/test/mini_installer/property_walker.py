# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import file_operations
import process_operations
import registry_operations


def Verify(property_dict, variable_expander):
  """Verifies the expectations in a property dict.

  Args:
    property_dict: A property dictionary mapping type names to expectations.
    variable_expander: A VariableExpander.

  Raises:
    AssertionError: If an expectation is not satisfied.
  """
  _Walk({
      'Files': file_operations.VerifyFileExpectation,
      'Processes': process_operations.VerifyProcessExpectation,
      'RegistryEntries': registry_operations.VerifyRegistryEntryExpectation,
  }, property_dict, variable_expander)


def Clean(property_dict, variable_expander):
  """Cleans machine state so that expectations will be satisfied.

  Args:
    property_dict: A property dictionary mapping type names to expectations.
    variable_expander: A VariableExpander.

  Raises:
    AssertionError: If an expectation is not satisfied.
  """
  _Walk({
      'Files': file_operations.CleanFile,
      'Processes': process_operations.CleanProcess,
      'RegistryEntries': registry_operations.CleanRegistryEntry,
  }, property_dict, variable_expander)


def _Walk(operations, property_dict, variable_expander):
  """Traverses |property_dict|, invoking |operations| for each
   expectation.

  Args:
    operations: A dictionary mapping property dict type names to functions.
    property_dict: A property dictionary mapping type names to expectations.
    variable_expander: A VariableExpander.
  """
  for type_name, expectations in property_dict.iteritems():
    operation = operations[type_name]
    for expectation_name, expectation_dict in expectations.iteritems():
      # Skip over expectations with conditions that aren't satisfied.
      if 'condition' in expectation_dict:
        condition = variable_expander.Expand(expectation_dict['condition'])
        if not _EvaluateCondition(condition):
          continue
      operation(expectation_name, expectation_dict, variable_expander)


def _EvaluateCondition(condition):
  """Evaluates |condition| using eval().

  Args:
    condition: A condition string.

  Returns:
    The result of the evaluated condition.
  """
  return eval(condition, {'__builtins__': {'False': False, 'True': True}})
