// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/font_list.h"

#import <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>

#include <utility>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"

namespace content {

namespace {

// Core Text-based localized family name lookup.
//
// This class is not thread-safe.
//
// This class caches some state for an efficient implementation of
// [NSFontManager localizedNameForFamily:face:] using the Core Text API.
class FontFamilyResolver {
 public:
  FontFamilyResolver() = default;
  ~FontFamilyResolver() = default;

  FontFamilyResolver(const FontFamilyResolver&) = delete;
  FontFamilyResolver& operator=(const FontFamilyResolver&) = delete;

  // Returns a localized font family name for the given family name.
  base::ScopedCFTypeRef<CFStringRef> CopyLocalizedFamilyName(
      CFStringRef family_name) {
    DCHECK(family_name != nullptr);

    CFDictionarySetValue(font_descriptor_attributes_.get(),
                         kCTFontFamilyNameAttribute, family_name);
    base::ScopedCFTypeRef<CTFontDescriptorRef> raw_descriptor(
        CTFontDescriptorCreateWithAttributes(
            font_descriptor_attributes_.get()));

    base::ScopedCFTypeRef<CFArrayRef> normalized_descriptors(
        CTFontDescriptorCreateMatchingFontDescriptors(
            raw_descriptor, mandatory_attributes_.get()));
    return CopyLocalizedFamilyNameFrom(family_name,
                                       normalized_descriptors.get());
  }

  // True if the font should be hidden from Chrome.
  //
  // On macOS 10.15, CTFontManagerCopyAvailableFontFamilyNames() filters hidden
  // fonts. This is not true on older version of macOS that Chrome still
  // supports. The unittest FontTest.GetFontListDoesNotIncludeHiddenFonts can be
  // used to determine when it's safe to slim down / remove this function.
  static bool IsHiddenFontFamily(CFStringRef family_name) {
    DCHECK_GT(CFStringGetLength(family_name), 0);
    return CFStringGetCharacterAtIndex(family_name, 0) == '.';
  }

 private:
  // Returns the first font descriptor matching the given family name.
  //
  // |descriptors| must be an array of CTFontDescriptors whose font family name
  // attribute is populated.
  //
  // Returns null if none of the descriptors match.
  static base::ScopedCFTypeRef<CTFontDescriptorRef> FindFirstWithFamilyName(
      CFStringRef family_name,
      CFArrayRef descriptors) {
    DCHECK(family_name != nullptr);
    DCHECK(descriptors != nullptr);

    CFIndex normalized_descriptor_count = CFArrayGetCount(descriptors);
    for (CFIndex i = 0; i < normalized_descriptor_count; ++i) {
      CTFontDescriptorRef descriptor =
          base::mac::CFCastStrict<CTFontDescriptorRef>(
              CFArrayGetValueAtIndex(descriptors, i));

      base::ScopedCFTypeRef<CFStringRef> descriptor_family_name(
          base::mac::CFCastStrict<CFStringRef>(CTFontDescriptorCopyAttribute(
              descriptor, kCTFontFamilyNameAttribute)));
      if (CFStringCompare(family_name, descriptor_family_name,
                          /*compareOptions=*/0) == kCFCompareEqualTo) {
        return base::ScopedCFTypeRef<CTFontDescriptorRef>(
            descriptor, base::scoped_policy::RETAIN);
      }
    }
    return base::ScopedCFTypeRef<CTFontDescriptorRef>(nullptr);
  }

  // Returns a localized font family name for the given family name.
  //
  // |descriptors| must be an array of CTFontDescriptors representing all
  // descriptors on the system matching the given family name.
  //
  // The given family name is returned as a fallback, if none of the descriptors
  // match the desired font family name.
  static base::ScopedCFTypeRef<CFStringRef> CopyLocalizedFamilyNameFrom(
      CFStringRef family_name,
      CFArrayRef descriptors) {
    DCHECK(family_name != nullptr);
    DCHECK(descriptors != nullptr);

    base::ScopedCFTypeRef<CTFontDescriptorRef> descriptor =
        FindFirstWithFamilyName(family_name, descriptors);
    if (descriptor == nullptr) {
      base::ScopedCFTypeRef<CFStringRef> fallback_return_value(
          CFStringCreateCopy(nullptr, family_name));
      DLOG(WARNING) << "Will use non-localized family name for font family: "
                    << family_name;
      return fallback_return_value;
    }

    base::ScopedCFTypeRef<CFStringRef> localized_family_name(
        base::mac::CFCastStrict<CFStringRef>(
            CTFontDescriptorCopyLocalizedAttribute(descriptor,
                                                   kCTFontFamilyNameAttribute,
                                                   /*language=*/nullptr)));
    return localized_family_name;
  }

  // Creates the set stored in |mandatory_attributes_|.
  static base::ScopedCFTypeRef<CFSetRef> CreateMandatoryAttributes() {
    CFStringRef set_values[] = {kCTFontFamilyNameAttribute};
    return base::ScopedCFTypeRef<CFSetRef>(CFSetCreate(
        kCFAllocatorDefault, reinterpret_cast<const void**>(set_values),
        base::size(set_values), &kCFTypeSetCallBacks));
  }

  // Creates the mutable dictionary stored in |font_descriptor_attributes_|.
  static base::ScopedCFTypeRef<CFMutableDictionaryRef>
  CreateFontDescriptorAttributes() {
    return base::ScopedCFTypeRef<CFMutableDictionaryRef>(
        CFDictionaryCreateMutable(kCFAllocatorDefault, /*capacity=*/1,
                                  &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks));
  }

  // Used for all CTFontDescriptorCreateMatchingFontDescriptors() calls.
  //
  // Caching this dictionary saves one dictionary creation per lookup.
  const base::ScopedCFTypeRef<CFSetRef> mandatory_attributes_ =
      CreateMandatoryAttributes();

  // Used for all CTFontDescriptorCreateMatchingFontDescriptors() calls.
  //
  // This dictionary has exactly one key, kCTFontFamilyNameAttribute. The value
  // associated with the key is overwritten as needed.
  //
  // Caching this dictionary saves one dictionary creation per lookup.
  const base::ScopedCFTypeRef<CFMutableDictionaryRef>
      font_descriptor_attributes_ = CreateFontDescriptorAttributes();
};

}  // namespace

std::unique_ptr<base::ListValue> GetFontList_SlowBlocking() {
  @autoreleasepool {
    FontFamilyResolver resolver;

    base::ScopedCFTypeRef<CFArrayRef> cf_family_names(
        CTFontManagerCopyAvailableFontFamilyNames());
    NSArray* family_names = base::mac::CFToNSCast(cf_family_names.get());

    // Maps localized font family names to non-localized names.
    NSMutableDictionary* family_name_map = [NSMutableDictionary
        dictionaryWithCapacity:CFArrayGetCount(cf_family_names)];
    for (NSString* family_name in family_names) {
      CFStringRef family_name_cf = base::mac::NSToCFCast(family_name);
      if (FontFamilyResolver::IsHiddenFontFamily(family_name_cf))
        continue;

      base::ScopedCFTypeRef<CFStringRef> cf_normalized_family_name =
          resolver.CopyLocalizedFamilyName(base::mac::NSToCFCast(family_name));
      family_name_map[base::mac::CFToNSCast(cf_normalized_family_name)] =
          family_name;
    }

    // The Apple documentation for CTFontManagerCopyAvailableFontFamilyNames
    // states that it returns family names sorted for user interface display.
    // https://developer.apple.com/documentation/coretext/1499494-ctfontmanagercopyavailablefontfa
    //
    // This doesn't seem to be the case, at least on macOS 10.15.3.
    NSArray* sorted_localized_family_names = [family_name_map
        keysSortedByValueUsingSelector:@selector(localizedStandardCompare:)];

    auto font_list = std::make_unique<base::ListValue>();
    for (NSString* localized_family_name in sorted_localized_family_names) {
      NSString* family_name = family_name_map[localized_family_name];
      auto font_item = std::make_unique<base::ListValue>();
      font_item->AppendString(base::SysNSStringToUTF16(family_name));
      font_item->AppendString(base::SysNSStringToUTF16(localized_family_name));
      font_list->Append(std::move(font_item));
    }

    return font_list;
  }
}

}  // namespace content
