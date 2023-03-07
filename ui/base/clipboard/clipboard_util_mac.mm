// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_util_mac.h"

#include "base/mac/foundation_util.h"
#import "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/notreached.h"

namespace ui {

NSString* const kUTTypeURLName = @"public.url-name";

namespace {

NSString* const kWebURLsWithTitlesPboardType = @"WebURLsWithTitlesPboardType";

// It's much more convenient to return an NSString than a
// base::ScopedCFTypeRef<CFStringRef>, since the methods on NSPasteboardItem
// require an NSString*.
NSString* UTIFromPboardType(NSString* type) {
  return [base::mac::CFToNSCast(UTTypeCreatePreferredIdentifierForTag(
      kUTTagClassNSPboardType, base::mac::NSToCFCast(type), kUTTypeData))
      autorelease];
}

bool ReadWebURLsWithTitlesPboardType(NSPasteboard* pboard,
                                     NSArray** urls,
                                     NSArray** titles) {
  NSArray* bookmarkPairs = base::mac::ObjCCast<NSArray>([pboard
      propertyListForType:UTIFromPboardType(kWebURLsWithTitlesPboardType)]);
  if (!bookmarkPairs)
    return false;

  if ([bookmarkPairs count] != 2)
    return false;

  NSArray* urlsArr = base::mac::ObjCCast<NSArray>(bookmarkPairs[0]);
  NSArray* titlesArr = base::mac::ObjCCast<NSArray>(bookmarkPairs[1]);

  if (!urlsArr || !titlesArr)
    return false;
  if ([urlsArr count] < 1)
    return false;
  if ([urlsArr count] != [titlesArr count])
    return false;

  for (id obj in urlsArr) {
    if (![obj isKindOfClass:[NSString class]])
      return false;
  }

  for (id obj in titlesArr) {
    if (![obj isKindOfClass:[NSString class]])
      return false;
  }

  *urls = urlsArr;
  *titles = titlesArr;
  return true;
}

// Returns the user-visible name of the file, optionally without any extension.
// If given a non-empty `file_url`, will always return a title.
NSString* DeriveTitleFromFilename(NSURL* file_url, bool strip_extension) {
  NSString* localized_name = nil;
  BOOL success = [file_url getResourceValue:&localized_name
                                     forKey:NSURLLocalizedNameKey
                                      error:nil];
  if (!success || !localized_name) {
    // For the case where the actual display name of a file cannot be obtained,
    // derive a quick-and-dirty version by swapping in "/" for ":", as that's
    // the most common difference between the last path component of a file and
    // how that file is presented to the user. See -[NSFileManager
    // displayNameAtPath:] for an example of macOS doing this. Also, given that
    // this is a failure case, don't bother trying to figure out the extension
    // situation.
    NSString* last_path_component = file_url.lastPathComponent;
    return [last_path_component stringByReplacingOccurrencesOfString:@":"
                                                          withString:@"/"];
  }

  if (!strip_extension) {
    return localized_name;
  }

  NSNumber* has_hidden_extension = nil;
  success = [file_url getResourceValue:&has_hidden_extension
                                forKey:NSURLHasHiddenExtensionKey
                                 error:nil];
  if (!success || !has_hidden_extension || has_hidden_extension.boolValue) {
    // If it's unknown if the extension is hidden, or if the extension is
    // already hidden, return the filename unaltered.
    return localized_name;
  }

  return [localized_name stringByDeletingPathExtension];
}

// A simple pair of URL with title. Valid if the `url` field is not null.
struct URLAndTitle {
  NSString* url = nil;
  NSString* title = nil;
};

// Returns a URL and title if standard URL and URL title types are present on
// the pasteboard item. Because the Finder and/or the core macOS drag code
// automatically turn .webloc file drags into standard URL types, .webloc file
// drags are also handled by this function.
URLAndTitle ExtractStandardURLAndTitle(NSPasteboardItem* item) {
  NSString* url = [item stringForType:NSPasteboardTypeURL];
  if (!url) {
    return {};
  }

  NSString* title = [item stringForType:kUTTypeURLName];

  if (!title) {
    // If there is no title on the drag, check to see if it's a URL drag
    // reconstituted from a Finder .webloc. If so, use the name of the file as
    // the title.
    NSString* file = [item stringForType:NSPasteboardTypeFileURL];
    if (file) {
      NSURL* file_url = [NSURL URLWithString:file].filePathURL;

      // The UTType for .webloc files is com.apple.web-internet-location, but
      // there is no official constant for that. However, that type does conform
      // to the generic "internet location" type (aka .inetloc), so check for
      // that.
      if (@available(macOS 11, *)) {
        UTType* type;
        if (![file_url getResourceValue:&type
                                 forKey:NSURLContentTypeKey
                                  error:nil]) {
          return {};
        }
        if (![type conformsToType:UTTypeInternetLocation]) {
          return {};
        }
      } else {
        NSString* type;
        if (![file_url getResourceValue:&type
                                 forKey:NSURLTypeIdentifierKey
                                  error:nil]) {
          return {};
        }
        if (![NSWorkspace.sharedWorkspace type:type
                                conformsToType:base::mac::CFToNSCast(
                                                   kUTTypeInternetLocation)]) {
          return {};
        }
      }

      title = DeriveTitleFromFilename(file_url, /*strip_extension=*/true);
    }
  }

  if (!title) {
    // If still no title, use the hostname as the last resort.
    title = [NSURL URLWithString:url].host;
  }

  return {.url = url, .title = title};
}

// Returns a URL and title if the pasteboard item is of a standard Microsoft
// Windows IShellLink-style .url file.
URLAndTitle ExtractURLFromURLFile(NSPasteboardItem* item) {
  NSString* file = [item stringForType:NSPasteboardTypeFileURL];
  if (!file) {
    return {};
  }
  NSURL* file_url = [NSURL URLWithString:file].filePathURL;

  if (@available(macOS 11, *)) {
    NSDictionary* resource_values;
    resource_values = [file_url
        resourceValuesForKeys:@[ NSURLFileSizeKey, NSURLContentTypeKey ]
                        error:nil];
    if (!resource_values) {
      return {};
    }

    NSNumber* file_size = resource_values[NSURLFileSizeKey];
    if (file_size.unsignedLongValue >
        ClipboardUtil::internal::kMaximumParsableFileSize) {
      return {};
    }

    UTType* type = resource_values[NSURLContentTypeKey];
    if (![type conformsToType:UTTypeInternetShortcut]) {
      return {};
    }
  } else {
    NSDictionary* resource_values;
    resource_values = [file_url
        resourceValuesForKeys:@[ NSURLFileSizeKey, NSURLTypeIdentifierKey ]
                        error:nil];
    if (!resource_values) {
      return {};
    }

    NSNumber* file_size = resource_values[NSURLFileSizeKey];
    if (file_size.unsignedLongValue >
        ClipboardUtil::internal::kMaximumParsableFileSize) {
      return {};
    }

    NSString* type = resource_values[NSURLTypeIdentifierKey];
    NSString* const kUTTypeInternetShortcut =
        @"com.microsoft.internet-shortcut";
    if (![NSWorkspace.sharedWorkspace type:type
                            conformsToType:kUTTypeInternetShortcut]) {
      return {};
    }
  }

  // Windows codepage 1252 (aka WinLatin1) is the best guess.
  NSString* contents =
      [NSString stringWithContentsOfURL:file_url
                               encoding:NSWindowsCP1252StringEncoding
                                  error:nil];
  if (!contents) {
    return {};
  }

  std::string found_url =
      ClipboardUtil::internal::ExtractURLFromURLFileContents(
          base::SysNSStringToUTF8(contents));
  if (found_url.empty()) {
    return {};
  }

  NSString* title = DeriveTitleFromFilename(file_url, /*strip_extension=*/true);

  return {.url = base::SysUTF8ToNSString(found_url), .title = title};
}

// Returns a URL and title if a string on the pasteboard item is formatted as a
// URL but doesn't actually have the URL type.
URLAndTitle ExtractURLFromStringValue(NSPasteboardItem* item) {
  NSString* string = [item stringForType:NSPasteboardTypeString];
  if (!string) {
    return {};
  }

  string = [string
      stringByTrimmingCharactersInSet:NSCharacterSet
                                          .whitespaceAndNewlineCharacterSet];

  // Check to see if this string is a valid URL; use GURL to do so. NSURL was
  // found in 2010 to not be strict enough; see https://crbug.com/43100. It's
  // unknown if things have changed since then, but there's no reason to revert.
  // FYI earlier code also allowed all "javascript:" and "data:" URLs as
  // "loosely validated". TODO(avi): If that "loosely validated" escape hatch
  // needed? If significant time goes by and no one complains, remove this TODO
  // and don't put that back in.
  GURL url(base::SysNSStringToUTF8(string));
  if (url.is_valid()) {
    // The hostname is the best that can be done for the title.
    return {.url = string, .title = base::SysUTF8ToNSString(url.host())};
  }

  return {};
}

// If there is a file URL on the pasteboard, returns that file as the URL and
// returns the file's name as the title.
URLAndTitle ExtractFileURL(NSPasteboardItem* item) {
  NSString* file = [item stringForType:NSPasteboardTypeFileURL];
  if (!file) {
    return {};
  }
  NSURL* file_url = [NSURL URLWithString:file].filePathURL;

  NSString* filename =
      DeriveTitleFromFilename(file_url, /*strip_extension=*/false);

  return {.url = file_url.absoluteString, .title = filename};
}

// Reads the given pasteboard, and returns URLs/titles found on it. If
// `include_files` is set, then any file references on the pasteboard will be
// returned as file URLs. Returns true if at least one URL was found on the
// pasteboard, and false if none were.
bool ReadURLItemsWithTitles(NSPasteboard* pboard,
                            NSArray** urls,
                            NSArray** titles) {
  NSMutableArray* urlsArr = [NSMutableArray array];
  NSMutableArray* titlesArr = [NSMutableArray array];

  NSArray* items = [pboard pasteboardItems];
  for (NSPasteboardItem* item : items) {
    NSString* url = [item stringForType:base::mac::CFToNSCast(kUTTypeURL)];
    NSString* title = [item stringForType:kUTTypeURLName];

    if (url) {
      [urlsArr addObject:url];
      if (title)
        [titlesArr addObject:title];
      else
        [titlesArr addObject:@""];
    }
  }

  if ([urlsArr count]) {
    *urls = urlsArr;
    *titles = titlesArr;
    return true;
  } else {
    return false;
  }
}

}  // namespace

UniquePasteboard::UniquePasteboard()
    : pasteboard_([[NSPasteboard pasteboardWithUniqueName] retain]) {}

UniquePasteboard::~UniquePasteboard() {
  [pasteboard_ releaseGlobally];

  if (base::mac::IsOS10_12()) {
    // On 10.12, move ownership to the autorelease pool rather than possibly
    // triggering -[NSPasteboard dealloc] here. This is a speculative workaround
    // for https://crbug.com/877979 where a call to __CFPasteboardDeallocate
    // from here is triggering "Semaphore object deallocated while in use".
    pasteboard_.autorelease();
  }
}

// static
base::scoped_nsobject<NSPasteboardItem> ClipboardUtil::PasteboardItemFromUrl(
    NSString* urlString,
    NSString* title) {
  DCHECK(urlString);
  if (!title)
    title = urlString;

  base::scoped_nsobject<NSPasteboardItem> item([[NSPasteboardItem alloc] init]);

  NSURL* url = [NSURL URLWithString:urlString];
  if ([url isFileURL] &&
      [[NSFileManager defaultManager] fileExistsAtPath:[url path]]) {
    [item setPropertyList:@[ [url path] ]
                  forType:UTIFromPboardType(NSFilenamesPboardType)];
  }

  // Set Safari's URL + title arrays Pboard type.
  NSArray* urlsAndTitles = @[ @[ urlString ], @[ title ] ];
  [item setPropertyList:urlsAndTitles
                forType:UTIFromPboardType(kWebURLsWithTitlesPboardType)];

  // Set NSURLPboardType. The format of the property list is divined from
  // Webkit's function PlatformPasteboard::setStringForType.
  // https://github.com/WebKit/webkit/blob/master/Source/WebCore/platform/mac/PlatformPasteboardMac.mm
  NSURL* base = [url baseURL];
  if (base) {
    [item setPropertyList:@[ [url relativeString], [base absoluteString] ]
                  forType:UTIFromPboardType(NSURLPboardType)];
  } else if (url) {
    [item setPropertyList:@[ [url absoluteString], @"" ]
                  forType:UTIFromPboardType(NSURLPboardType)];
  }

  [item setString:urlString forType:NSPasteboardTypeString];
  [item setString:urlString forType:base::mac::CFToNSCast(kUTTypeURL)];
  [item setString:title forType:kUTTypeURLName];
  return item;
}

// static
base::scoped_nsobject<NSPasteboardItem> ClipboardUtil::PasteboardItemFromUrls(
    NSArray* urls,
    NSArray* titles) {
  base::scoped_nsobject<NSPasteboardItem> item([[NSPasteboardItem alloc] init]);

  // Set Safari's URL + title arrays Pboard type.
  NSArray* urlsAndTitles = @[ urls, titles ];
  [item setPropertyList:urlsAndTitles
                forType:UTIFromPboardType(kWebURLsWithTitlesPboardType)];

  return item;
}

// static
base::scoped_nsobject<NSPasteboardItem> ClipboardUtil::PasteboardItemFromString(
    NSString* string) {
  base::scoped_nsobject<NSPasteboardItem> item([[NSPasteboardItem alloc] init]);
  [item setString:string forType:NSPasteboardTypeString];
  return item;
}

//static
NSString* ClipboardUtil::GetTitleFromPasteboardURL(NSPasteboard* pboard) {
  return [pboard stringForType:kUTTypeURLName];
}

//static
NSString* ClipboardUtil::GetURLFromPasteboardURL(NSPasteboard* pboard) {
  return [pboard stringForType:base::mac::CFToNSCast(kUTTypeURL)];
}

// static
NSString* ClipboardUtil::UTIForPasteboardType(NSString* type) {
  return UTIFromPboardType(type);
}

// static
NSString* ClipboardUtil::UTIForWebURLsAndTitles() {
  return UTIFromPboardType(kWebURLsWithTitlesPboardType);
}

// static
void ClipboardUtil::AddDataToPasteboard(NSPasteboard* pboard,
                                        NSPasteboardItem* item) {
  NSSet* oldTypes = [NSSet setWithArray:[pboard types]];
  NSMutableSet* newTypes = [NSMutableSet setWithArray:[item types]];
  [newTypes minusSet:oldTypes];

  [pboard addTypes:[newTypes allObjects] owner:nil];
  for (NSString* type in newTypes) {
    // Technically, the object associated with |type| might be an NSString or a
    // property list. It doesn't matter though, since the type gets pulled from
    // and shoved into an NSDictionary.
    [pboard setData:[item dataForType:type] forType:type];
  }
}

// static
bool ClipboardUtil::URLsAndTitlesFromPasteboard(NSPasteboard* pboard,
                                                NSArray** urls,
                                                NSArray** titles) {
  return ReadWebURLsWithTitlesPboardType(pboard, urls, titles) ||
         ReadURLItemsWithTitles(pboard, urls, titles);
}

// static
NSPasteboard* ClipboardUtil::PasteboardFromBuffer(ClipboardBuffer buffer) {
  NSString* buffer_type = nil;
  switch (buffer) {
    case ClipboardBuffer::kCopyPaste:
      buffer_type = NSGeneralPboard;
      break;
    case ClipboardBuffer::kDrag:
      buffer_type = NSDragPboard;
      break;
    case ClipboardBuffer::kSelection:
      NOTREACHED();
      break;
  }

  return [NSPasteboard pasteboardWithName:buffer_type];
}

// static
NSString* ClipboardUtil::GetHTMLFromRTFOnPasteboard(NSPasteboard* pboard) {
  NSData* rtfData = [pboard dataForType:NSRTFPboardType];
  if (!rtfData)
    return nil;

  NSAttributedString* attributed =
      [[[NSAttributedString alloc] initWithRTF:rtfData
                            documentAttributes:nil] autorelease];
  NSData* htmlData =
      [attributed dataFromRange:NSMakeRange(0, [attributed length])
             documentAttributes:@{
               NSDocumentTypeDocumentAttribute : NSHTMLTextDocumentType
             }
                          error:nil];

  // According to the docs, NSHTMLTextDocumentType is UTF8.
  return [[[NSString alloc] initWithData:htmlData
                                encoding:NSUTF8StringEncoding] autorelease];
}

}  // namespace ui
