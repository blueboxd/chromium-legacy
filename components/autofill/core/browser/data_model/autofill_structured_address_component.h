// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_COMPONENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_COMPONENT_H_

#include <map>
#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "components/autofill/core/browser/field_types.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill {
namespace structured_address {

enum class VerificationStatus {
  // No verification status assigned.
  kNoStatus,
  // The user used the autofill settings to verify and store this token.
  kUserVerified,
  // The value was observed in a form transmission.
  kObserved,
  // Value was built from its subcomponents.
  kFormatted,
  // The value token was parsed from a parent token.
  kParsed,
};

// An AddressComponent is a tree structure that represents a semi-structured
// address token. Such an address token can either be an atomic leaf node or
// have a set of children, each representing a more granular subtoken of the
// component.
//
// An AddressComponent has a string representation stored in |value_| and a
// VerificationStatus stored in |verification_status_|.
// The latter indicates if the value was user-verified, observed in a form
// submission event, parsed from its parent component or was formatted from its
// child components.
//
// In a proper component tree, each AddressComponent has a unique
// ServerFieldType. Additionally, an AddressComponent may be associated with a
// list of additional field types that allow for retrieving and setting the
// Component's value in specific formats. For example, NAME_MIDDLE may be the
// storage type and NAME_MIDDLE_INITIAL is an additional field type.
//
// The usage pattern of such an address tree is as follows:
//
//  * Create a tree from an observed form submission or a profile editing or
//  creation event in the Chrome settings. It is assumed that the created
//  tree does not have values for competing field types. Two types are competing
//  iff they are on a common root-to-leaf path. For example, an imported profile
//  with a value for NAME_FULL and NAME_LAST has conflicting types that
//  carry redundant information.
//
//  * After the creation of the tree, the values of unassigned nodes in the tree
//  are deducted from the values of assigned nodes. This happens by parsing
//  (taking a string and splitting it into components) or by formatting (taking
//  one or multiple strings and combining them into one string).
//
//  * After the completion, there should be no need to modify the tree.
//
//  * A tree may be mergeable with another tree of the same type. This
//  operation incorporates complementing observations. For example, in the first
//  tree NAME_FIRST, NAME_MIDDLE and NAME_LAST may be parsed from an observed
//  unstructured name (NAME_FULL). The second tree may be built from observing
//  the structured name, and contain observed NAME_FIRST, NAME_MIDDLE and
//  NAME_LAST values but only a formatted NAME_FULL value.
class AddressComponent {
 public:
  // Constructor for an atomic root node.
  explicit AddressComponent(ServerFieldType storage_type);

  // Constructor for an atomic leaf node.
  explicit AddressComponent(ServerFieldType storage_type,
                            AddressComponent* parent);

  // Constructor for a compound child node.
  AddressComponent(ServerFieldType storage_type,
                   AddressComponent* parent,
                   std::vector<AddressComponent*> subcomponents);

  // Disallows copies since they are not needed in the current Autofill design.
  AddressComponent(const AddressComponent& other) = delete;

  virtual ~AddressComponent();

  // Assignment operator that works recursively down the tree and assigns the
  // |value_| and |verification_status_| of every node in right to the
  // corresponding nodes in |this|. For an assignment it is required that both
  // nodes have the same |storage_type_|.
  AddressComponent& operator=(const AddressComponent& right);

  // Comparison operator that works recursively down the tree.
  bool operator==(const AddressComponent& right) const;

  // Inequality operator that works recursively down the tree.
  bool operator!=(const AddressComponent& right) const;

  // Returns the autofill storage type stored in |storage_type_|.
  ServerFieldType GetStorageType() const;

  // Returns the string representation of |storage_type_|.
  std::string GetStorageTypeName() const;

  // Returns the value verification status of the component's value;
  VerificationStatus GetVerificationStatus() const;

  // Returns true if the component has no subcomponents.
  bool IsAtomic() const;

  // Returns a constant reference to |value_.value()|. If the value is not
  // assigned, an empty string is returned.
  const base::string16& GetValue() const;

  // Returns true if the value of this AddressComponent is assigned.
  bool IsValueAssigned() const;

  // Sets the value corresponding to the storage type of this AddressComponent.
  void SetValue(base::string16 value, VerificationStatus status);

  // Sets the value to an empty string, marks it unassigned and sets the
  // verification status to |kNoStatus|.
  void UnsetValue();

  // The method sets the value of the current node if its |storage_type_| is
  // |type| or if |ConvertAndGetTheValueForAdditionalFieldTypeName()| supports
  // retrieving |type|. Otherwise, the call is delegated recursively to the
  // node's children.
  // Returns true if the |value_| and |verification_status_| were successfully
  // set for this or an ancestor node with the storage type |type|. If
  // |invalidate_child_nodes|, all child nodes of the assigned node are
  // unassigned. If |invalidate_parent_nodes|, all ancestor nodes of the
  // assigned node as unassigned.
  bool SetValueForTypeIfPossible(const ServerFieldType& type,
                                 const base::string16& value,
                                 const VerificationStatus& verification_status,
                                 bool invalidate_child_nodes = false,
                                 bool invalidate_parent_nodes = false);

  // Same as |SetValueForTypeIfPossible()| but the type is supplied in the
  // corresponding string representation.
  bool SetValueForTypeIfPossible(const std::string& type_name,
                                 const base::string16& value,
                                 const VerificationStatus& verification_status,
                                 bool invalidate_child_nodes = false,
                                 bool invalidate_parent_nodes = false);

  // Convenience method to get the value of |type|.
  // Returns an empty string if |type| is not supported.
  base::string16 GetValueForType(const ServerFieldType& type) const;

  // Convenience method to get the value of |type| identified by its string
  // representation name. Returns an empty string if |type| is not supported.
  base::string16 GetValueForType(const std::string& type) const;

  // Convenience method to get the verification status of |type|.
  // Returns |VerificationStatus::kNoStatus| if |type| is not supported.
  VerificationStatus GetVerificationStatusForType(
      const ServerFieldType& type) const;

  // Convenience method to get the verification status of |type| identified by
  // its name. Returns |VerificationStatus::kNoStatus| if |type| is not
  // supported.
  VerificationStatus GetVerificationStatusForType(
      const std::string& type) const;

  // Get the value and status of a |type|,
  // Returns false if the |type| is not supported by the structure.
  // The method returns |value_| and |validation_status_| of the current node if
  // its |storage_type_| is |type| or if
  // |ConvertAndSetTheValueForAdditionalFieldTypeName()| supports setting
  // |type|. Otherwise, the call is delegated recursively to the node's
  // children. Returns false if the neither the node or one of its ancestors
  // supports |type|.
  bool GetValueAndStatusForTypeIfPossible(const ServerFieldType& type,
                                          base::string16* value,
                                          VerificationStatus* status) const;

  // Get the value and status of a |type| identified by its name.
  // Returns false if the |type| is not supported by the structure.
  bool GetValueAndStatusForTypeIfPossible(const std::string& type_name,
                                          base::string16* value,
                                          VerificationStatus* status) const;

  // Returns true if the |value| and |verification_status| were successfully
  // unset for |type|.
  bool UnsetValueForTypeIfSupported(const ServerFieldType& type);

  // Parses |value_| to assign values to the subcomponents.
  // The method uses 3 stages:
  //
  // * Use |ParseValueAndAssignSubcomponentsByMethod()|. This stage exists
  // to catch special cases and may fail. The method is virtual and can be
  // implemented on the type level.
  //
  // * Use |ParseValueAndAssignSubcomponentsByRegularExpressions()|. This stage
  // uses a list of regular expressions acquired by the virtual method
  // |GetParseRegularExpressionsByRelevance()|. This stage my fail.
  //
  // * Use |ParseValueAndAssignSubcomponentsByFallbackMethod()| as the last
  // resort to parse |value_|. This method must produce a valid result.
  void ParseValueAndAssignSubcomponents();

  // This methods populated the unassigned entries in the subtree of this node
  // by either parsing unknown values for subcomponents from their parents, or
  // vice versa, formatting unknown values from known subcomponents. The method
  // is virtual and can be reimplemented on the type level.
  virtual void RecursivelyCompleteTree();

  // Completes the full tree by calling |RecursivelyCompleteTree()| starting
  // form the root node. Returns true if the completion was successful.
  bool CompleteFullTree();

  // Checks if a tree is completable in the sense that there are no conflicting
  // observed or verified types. This means that there is not more than one
  // observed or verified node on any root-to-leaf path in the tree.
  bool IsTreeCompletable();

  // Recursively adds the supported types to the set. Calls
  // |GetAdditionalSupportedFieldTypes()| to add field types.
  void GetSupportedTypes(ServerFieldTypeSet* supported_types) const;

  // Adds the additional supported field types to |supported_types|.
  // The method should DCHECK that the added types are not part of the set yet.
  virtual void GetAdditionalSupportedFieldTypes(
      ServerFieldTypeSet* supported_types) const {}

  // Unassigns all nodes with parsed or formatted values.
  void UnsetParsedAndFormattedValuesInEntireTree();

  // Unassigns all nodes with parsed or formatted values.
  void RecursivelyUnsetParsedAndFormattedValues();

#ifdef UNIT_TEST
  // Initiates the formatting of the values from the subcomponents.
  void FormatValueFromSubcomponentsForTesting() {
    FormatValueFromSubcomponents();
  }

  // Returns the best format string for testing.
  base::string16 GetBestFormatStringForTesting() {
    return GetBestFormatString();
  }

  // Returns the parse expressions by relevance for testing.
  std::vector<const RE2*> GetParseRegularExpressionsByRelevanceForTesting() {
    return GetParseRegularExpressionsByRelevance();
  }

  // Returns a reference to the root node of the tree for testing.
  AddressComponent& GetRootNodeForTesting() { return GetRootNode(); }

  // Replaces placeholder values in the best format string with the
  // corresponding values.
  base::string16 GetReplacedPlaceholderTypesWithValuesForTesting() const {
    return ReplacePlaceholderTypesWithValues(GetBestFormatString());
  }

  // Returns a vector containing the |storage_types_| of all direct
  // subcomponents.
  std::vector<ServerFieldType> GetSubcomponentTypesForTesting() const {
    return GetSubcomponentTypes();
  }

#endif

 protected:
  // Returns a vector containing the |storage_types_| of all direct
  // subcomponents.
  std::vector<ServerFieldType> GetSubcomponentTypes() const;

  // Heuristic method to get the best suited format string.
  // This method is virtual and can be reimplemented for each type.
  virtual base::string16 GetBestFormatString() const;

  // Returns pointers to regular expressions sorted by their relevance.
  // This method is virtual and can be reimplemented for each type.
  virtual std::vector<const RE2*> GetParseRegularExpressionsByRelevance() const;

  // Method to parse |value_| into the values of |subcomponents_|. The
  // purpose of this method is to cover special cases. This method returns true
  // on success and is allowed to fail. On failure, the |subcomponents_| are not
  // altered.
  virtual bool ParseValueAndAssignSubcomponentsByMethod();

  // This method parses |value_| to assign values to the subcomponents.
  // The method is virtual and can be reimplemented per type.
  // It must succeed.
  virtual void ParseValueAndAssignSubcomponentsByFallbackMethod();

  // This method is used to set the value given by a type different than the
  // storage type. It must implement the conversion logic specific to each type.
  // It returns true if conversion logic exists and the type can be set.
  virtual bool ConvertAndSetValueForAdditionalFieldTypeName(
      const std::string& field_type_name,
      const base::string16& value,
      const VerificationStatus& status);

  // This method is used to retrieve the value for a supported field type
  // different from the storage type. It must implement the conversion logic
  // specific to each type. It returns true if the type is supported and the
  // value can be written back to value.
  // The method must handle |nullptr|s for both the value and status.
  virtual bool ConvertAndGetTheValueForAdditionalFieldTypeName(
      const std::string& field_type_name,
      base::string16* value) const;

  // Clears all parsed and formatted values.
  void ClearAllParsedAndFormattedValues();

 private:
  // Returns a reference to the constant root node of the tree.
  const AddressComponent& GetRootNode() const;

  // Returns a reference to the root node of the tree.
  AddressComponent& GetRootNode();

  // Unsets the node and all of its children.
  void UnsetAddressComponentAndItsSubcomponents();

  // Unsets the children of a node.
  void UnsetSubcomponents();

  // Determines the |value_| from the values of the subcomponents by using the
  // most suitable format string determined by |GetBestFormatString()|.
  void FormatValueFromSubcomponents();

  // Replaces placeholder values with the corresponding values.
  base::string16 ReplacePlaceholderTypesWithValues(
      const base::string16& format) const;

  // Replaces placeholder values with the corresponding values.
  base::string16 ReplacePlaceholderTypesWithValuesRegexVersion(
      const base::string16& format) const;

  // This method uses regular expressions acquired by
  // |GetParseRegularExpressionsByRelevance| to parse |value_| into the values
  // of the subcomponents. Returns true on success and is allowed to fail.
  bool ParseValueAndAssignSubcomponentsByRegularExpressions();

  // Returns the maximum number of components with assigned values on the path
  // from the component to a leaf node.
  int MaximumNumberOfAssignedAddressComponentsOnNodeToLeafPaths() const;

  // The unstructured value of this component.
  base::Optional<base::string16> value_;

  // The verification status of |value_| indicates the certainty of the value
  // to be correct.
  VerificationStatus value_verification_status_;

  // The storable Autofill type of the component.
  const ServerFieldType storage_type_;

  // A vector of pointers to the subcomponents.
  std::vector<AddressComponent*> subcomponents_;

  // A pointer to the parent node. It is set to nullptr if the node is the root
  // node of the AddressComponent tree.
  AddressComponent* const parent_;
};

}  // namespace structured_address

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_STRUCTURED_ADDRESS_COMPONENT_H_
