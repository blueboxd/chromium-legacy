// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_SELECTION_UTILS_H_
#define UI_BASE_X_SELECTION_UTILS_H_

#include <stddef.h>
#include <map>

#include "base/component_export.h"
#include "base/memory/ref_counted_memory.h"
#include "ui/gfx/x/x11.h"

namespace ui {
class SelectionData;

COMPONENT_EXPORT(UI_BASE_X) extern const char kString[];
COMPONENT_EXPORT(UI_BASE_X) extern const char kText[];
COMPONENT_EXPORT(UI_BASE_X) extern const char kUtf8String[];

// Returns a list of all text atoms that we handle.
COMPONENT_EXPORT(UI_BASE_X) std::vector<::Atom> GetTextAtomsFrom();

COMPONENT_EXPORT(UI_BASE_X) std::vector<::Atom> GetURLAtomsFrom();

COMPONENT_EXPORT(UI_BASE_X) std::vector<::Atom> GetURIListAtomsFrom();

// Places the intersection of |desired| and |offered| into |output|.
COMPONENT_EXPORT(UI_BASE_X)
void GetAtomIntersection(const std::vector<::Atom>& desired,
                         const std::vector<::Atom>& offered,
                         std::vector<::Atom>* output);

// Takes the raw bytes of the base::string16 and copies them into |bytes|.
COMPONENT_EXPORT(UI_BASE_X)
void AddString16ToVector(const base::string16& str,
                         std::vector<unsigned char>* bytes);

// Tokenizes and parses the Selection Data as if it is a URI List.
COMPONENT_EXPORT(UI_BASE_X)
std::vector<std::string> ParseURIList(const SelectionData& data);

COMPONENT_EXPORT(UI_BASE_X)
std::string RefCountedMemoryToString(
    const scoped_refptr<base::RefCountedMemory>& memory);

COMPONENT_EXPORT(UI_BASE_X)
base::string16 RefCountedMemoryToString16(
    const scoped_refptr<base::RefCountedMemory>& memory);

///////////////////////////////////////////////////////////////////////////////

// Represents the selection in different data formats. Binary data passed in is
// assumed to be allocated with new char[], and is owned by SelectionFormatMap.
class COMPONENT_EXPORT(UI_BASE_X) SelectionFormatMap {
 public:
  // Our internal data store, which we only expose through iterators.
  typedef std::map< ::Atom, scoped_refptr<base::RefCountedMemory> > InternalMap;
  typedef InternalMap::const_iterator const_iterator;

  SelectionFormatMap();
  SelectionFormatMap(const SelectionFormatMap& other);
  ~SelectionFormatMap();
  // Copy and assignment deliberately open.

  // Adds the selection in the format |atom|. Ownership of |data| is passed to
  // us.
  void Insert(::Atom atom, const scoped_refptr<base::RefCountedMemory>& item);

  // Returns the first of the requested_types or NULL if missing.
  ui::SelectionData GetFirstOf(
      const std::vector< ::Atom>& requested_types) const;

  // Returns all the selected types.
  std::vector< ::Atom> GetTypes() const;

  // Pass through to STL map. Only allow non-mutation access.
  const_iterator begin() const { return data_.begin(); }
  const_iterator end() const { return data_.end(); }
  const_iterator find(::Atom atom) const { return data_.find(atom); }
  size_t size() const { return data_.size(); }

 private:
  InternalMap data_;
};

///////////////////////////////////////////////////////////////////////////////

// A holder for data with optional X11 deletion semantics.
class COMPONENT_EXPORT(UI_BASE_X) SelectionData {
 public:
  // |atom_cache| is still owned by caller.
  SelectionData();
  SelectionData(::Atom type,
                const scoped_refptr<base::RefCountedMemory>& memory);
  SelectionData(const SelectionData& rhs);
  ~SelectionData();
  SelectionData& operator=(const SelectionData& rhs);

  bool IsValid() const;
  ::Atom GetType() const;
  const unsigned char* GetData() const;
  size_t GetSize() const;

  // If |type_| is a string type, convert the data to UTF8 and return it.
  std::string GetText() const;

  // If |type_| is the HTML type, returns the data as a string16. This detects
  // guesses the character encoding of the source.
  base::string16 GetHtml() const;

  // Assigns the raw data to the string.
  void AssignTo(std::string* result) const;
  void AssignTo(base::string16* result) const;

 private:
  ::Atom type_;
  scoped_refptr<base::RefCountedMemory> memory_;
};

}  // namespace ui

#endif  // UI_BASE_X_SELECTION_UTILS_H_
