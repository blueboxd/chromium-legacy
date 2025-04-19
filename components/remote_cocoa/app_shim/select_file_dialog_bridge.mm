// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/select_file_dialog_bridge.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <stddef.h>

#include "base/apple/bridging.h"
#import "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_util.h"
#include "base/i18n/case_conversion.h"
#import "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/hang_watcher.h"
#include "base/threading/thread_restrictions.h"
#import "ui/base/cocoa/controls/textfield_utils.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#import "ui/base/l10n/l10n_util_mac.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

const int kFileTypePopupTag = 1234;

CFStringRef CreateUTIFromExtension(const base::FilePath::StringType& ext) {
  base::apple::ScopedCFTypeRef<CFStringRef> ext_cf(
      base::SysUTF8ToCFStringRef(ext));
  return UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension,
                                               ext_cf.get(), nullptr);
}

NSString* GetDescriptionFromExtension(const base::FilePath::StringType& ext) {
  CFStringRef uti(CreateUTIFromExtension(ext));
  NSString* description((__bridge NSString*)UTTypeCopyDescription(uti));

  if (description.length) {
    return description;
  } else {
    base::apple::ScopedCFTypeRef<CFStringRef> ext_cf =
        base::SysUTF8ToCFStringRef(ext);
    base::apple::ScopedCFTypeRef<CFStringRef> uti(
        UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension,
                                              ext_cf.get(),
                                              /*inConformingToUTI=*/nullptr));
    NSString* description =
        base::apple::CFToNSOwnershipCast(UTTypeCopyDescription(uti.get()));

    if (description && description.length) {
      return description;
    }
  }

  // In case no description is found, create a description based on the
  // unknown extension type (i.e. if the extension is .qqq, the we create
  // a description "QQQ File (.qqq)").
  std::u16string ext_name = base::UTF8ToUTF16(ext);
  return l10n_util::GetNSStringF(IDS_APP_SAVEAS_EXTENSION_FORMAT,
                                 base::i18n::ToUpper(ext_name), ext_name);
}

NSView* CreateAccessoryView() {
  static constexpr CGFloat kControlPadding = 2;

  NSView* view(
      [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 350, 60)]);

  // Create the label and center it vertically.
  NSTextField* label = [TextFieldUtils
      labelWithString:l10n_util::GetNSString(
                          IDS_SAVE_PAGE_FILE_FORMAT_PROMPT_MAC)];
  [label sizeToFit];
  NSRect label_frame = [label frame];
  label_frame.origin =
      NSMakePoint(kControlPadding, NSMidY([view frame]) - NSMidY(label_frame));
  [label setFrame:label_frame];
  [view addSubview:label];

  // Create the pop-up button, positioning it to the right of the label.
  // Its X position needs to be slightly below the label's, so that the text
  // baselines are aligned.
  NSPopUpButton* pop_up_button([[NSPopUpButton alloc]
      initWithFrame:NSMakeRect(NSWidth(label_frame) + kControlPadding,
                               NSMinY(label_frame) - 5, 230, 25)
          pullsDown:NO]);
  [pop_up_button setTag:kFileTypePopupTag];
  [view addSubview:pop_up_button];

  // Resize the containing view to fit the controls.
  CGFloat total_width = NSMaxX([pop_up_button frame]);
  NSRect view_frame = [view frame];
  view_frame.size.width = total_width + kControlPadding;
  [view setFrame:view_frame];

  return view;
}

NSSavePanel* __unsafe_unretained g_last_created_panel_for_testing = nil;

}  // namespace

// A bridge class to act as the modal delegate to the save/open sheet and send
// the results to the C++ class.
@interface SelectFileDialogDelegate : NSObject <NSOpenSavePanelDelegate>
@end

// Target for NSPopupButton control in file dialog's accessory view.
@interface ExtensionDropdownHandler : NSObject {
 @private
  // The file dialog to which this target object corresponds. Weak reference
  // since the dialog_ will stay alive longer than this object.
  NSSavePanel* _dialog;

  // An array whose each item corresponds to an array of different extensions in
  // an extension group.
  NSArray* _fileTypeLists;
}

- (instancetype)initWithDialog:(NSSavePanel*)dialog
                 fileTypeLists:(NSArray*)fileTypeLists;

- (void)popupAction:(id)sender;
@end

@implementation SelectFileDialogDelegate

- (BOOL)panel:(id)sender validateURL:(NSURL*)url error:(NSError**)outError {
  // Refuse to accept users closing the dialog with a key repeat, since the key
  // may have been first pressed while the user was looking at insecure content.
  // See https://crbug.com/637098.
  if ([[NSApp currentEvent] type] == NSEventTypeKeyDown &&
      [[NSApp currentEvent] isARepeat]) {
    return NO;
  }

  return YES;
}

@end

@implementation ExtensionDropdownHandler

- (instancetype)initWithDialog:(NSSavePanel*)dialog
                 fileTypeLists:(NSArray*)fileTypeLists {
  if ((self = [super init])) {
    _dialog = dialog;
    _fileTypeLists = fileTypeLists;
  }
  return self;
}

- (void)popupAction:(id)sender {
  NSUInteger index = [sender indexOfSelectedItem];
  if (index < [_fileTypeLists count]) {
    // For save dialogs, this causes the first item in the allowedFileTypes
    // array to be used as the extension for the save panel.
    [_dialog setAllowedFileTypes:[_fileTypeLists objectAtIndex:index]];
  } else {
    // The user selected "All files" option.
    [_dialog setAllowedFileTypes:nil];
  }
#pragma clang diagnostic pop
}

@end

namespace remote_cocoa {

using mojom::SelectFileDialogType;
using mojom::SelectFileTypeInfoPtr;

SelectFileDialogBridge::SelectFileDialogBridge(NSWindow* owning_window)
    : owning_window_(owning_window), weak_factory_(this) {}

SelectFileDialogBridge::~SelectFileDialogBridge() {
  // If we never executed our callback, then the panel never closed. Cancel it
  // now.
  if (show_callback_) {
    [panel_ cancel:panel_];
  }

  // Balance the setDelegate called during Show.
  panel_.delegate = nil;
}

void SelectFileDialogBridge::Show(
    SelectFileDialogType type,
    const std::u16string& title,
    const base::FilePath& default_path,
    SelectFileTypeInfoPtr file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    ShowCallback callback) {
  // Never consider the current WatchHangsInScope as hung. There was most likely
  // one created in ThreadControllerWithMessagePumpImpl::DoWork(). The current
  // hang watching deadline is not valid since the user can take unbounded time
  // to select a file. HangWatching will resume when the next task
  // or event is pumped in MessagePumpCFRunLoop so there is no need to
  // reactivate it. You can see the function comments for more details.
  base::HangWatcher::InvalidateActiveExpectations();

  show_callback_ = std::move(callback);
  type_ = type;
  // Note: we need to retain the dialog as |owning_window_| can be null.
  // (See https://crbug.com/41052845.)
  if (type_ == SelectFileDialogType::kSaveAsFile) {
    panel_ = [NSSavePanel savePanel];
  } else {
    panel_ = [NSOpenPanel openPanel];
  }
  g_last_created_panel_for_testing = panel_;

  if (!title.empty()) {
    panel_.message = base::SysUTF16ToNSString(title);
  }

  NSString* default_dir = nil;
  NSString* default_filename = nil;
  if (!default_path.empty()) {
    // The file dialog is going to do a ton of stats anyway. Not much
    // point in eliminating this one.
    base::ScopedAllowBlocking allow_blocking;
    if (base::DirectoryExists(default_path)) {
      default_dir = base::SysUTF8ToNSString(default_path.value());
    } else {
      default_dir = base::SysUTF8ToNSString(default_path.DirName().value());
      default_filename =
          base::SysUTF8ToNSString(default_path.BaseName().value());
    }
  }

  const bool keep_extension_visible =
      file_types ? file_types->keep_extension_visible : false;
  if (type_ != SelectFileDialogType::kFolder &&
      type_ != SelectFileDialogType::kUploadFolder &&
      type_ != SelectFileDialogType::kExistingFolder) {
    if (file_types) {
      SetAccessoryView(
          std::move(file_types), file_type_index, default_extension,
          /*is_save_panel=*/type_ == SelectFileDialogType::kSaveAsFile);
    } else {
      // If no type_ info is specified, anything goes.
      panel_.allowsOtherFileTypes = YES;
    }
  }

  if (type_ == SelectFileDialogType::kSaveAsFile) {
    // When file extensions are hidden and removing the extension from
    // the default filename gives one which still has an extension
    // that OS X recognizes, it will get confused and think the user
    // is trying to override the default extension. This happens with
    // filenames like "foo.tar.gz" or "ball.of.tar.png". Work around
    // this by never hiding extensions in that case.
    base::FilePath::StringType penultimate_extension =
        default_path.RemoveFinalExtension().FinalExtension();
    if (!penultimate_extension.empty() || keep_extension_visible) {
      panel_.extensionHidden = NO;
    } else {
      panel_.extensionHidden = YES;
      panel_.canSelectHiddenExtension = YES;
    }

    if (@available(macOS 10.9, *)) {
      // The tag autosetter in macOS is not reliable (see
      // https://crbug.com/1510399). Explicitly set the `showsTagField` property
      // as a signal to macOS that we will handle all the file tagging; a
      // side-effect of setting the property to any value is that it turns off
      // the tag autosetter.
      panel_.showsTagField = YES;
    }
  } else {
    // This does not use ObjCCast because the underlying object could be a
    // non-exported AppKit type (https://crbug.com/41477018).
    NSOpenPanel* open_dialog = static_cast<NSOpenPanel*>(panel_);

    if (type_ == SelectFileDialogType::kOpenMultiFile) {
      open_dialog.allowsMultipleSelection = YES;
    } else {
      open_dialog.allowsMultipleSelection = NO;
    }

    if (type_ == SelectFileDialogType::kFolder ||
        type_ == SelectFileDialogType::kUploadFolder ||
        type_ == SelectFileDialogType::kExistingFolder) {
      open_dialog.canChooseFiles = NO;
      open_dialog.canChooseDirectories = YES;

      if (type_ == SelectFileDialogType::kFolder) {
        open_dialog.canCreateDirectories = YES;
      } else {
        open_dialog.canCreateDirectories = NO;
      }

      NSString* prompt =
          (type_ == SelectFileDialogType::kUploadFolder)
              ? l10n_util::GetNSString(IDS_SELECT_UPLOAD_FOLDER_BUTTON_TITLE)
              : l10n_util::GetNSString(IDS_SELECT_FOLDER_BUTTON_TITLE);
      open_dialog.prompt = prompt;
    } else {
      open_dialog.canChooseFiles = YES;
      open_dialog.canChooseDirectories = NO;
    }

    delegate_ = [[SelectFileDialogDelegate alloc] init];
    open_dialog.delegate = delegate_;
  }
  if (default_dir) {
    panel_.directoryURL = [NSURL fileURLWithPath:default_dir];
  }
  if (default_filename) {
    panel_.nameFieldStringValue = default_filename;
  }

  // Ensure that |callback| (rather than |this|) be retained by the block.
  auto ended_callback = base::BindRepeating(
      &SelectFileDialogBridge::OnPanelEnded, weak_factory_.GetWeakPtr());

  NSWindow* sheet_parent = owning_window_;
  if (NativeWidgetMacNSWindow* sheet_parent_widget_window =
          base::apple::ObjCCast<NativeWidgetMacNSWindow>(sheet_parent)) {
    sheet_parent = [sheet_parent_widget_window preferredSheetParent];
  }
  [panel_ beginSheetModalForWindow:sheet_parent
                 completionHandler:^(NSInteger result) {
                   ended_callback.Run(result != NSModalResponseOK);
                 }];
}

void SelectFileDialogBridge::SetAccessoryView(
    SelectFileTypeInfoPtr file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    bool is_save_panel) {
  DCHECK(file_types);
  NSView* accessory_view = CreateAccessoryView();

  NSPopUpButton* popup = [accessory_view viewWithTag:kFileTypePopupTag];
  DCHECK(popup);

  // Create an array with each item corresponding to an array of different
  // extensions in an extension group.
  NSMutableArray* file_type_lists = [NSMutableArray array];
  int default_extension_index = -1;
  for (size_t i = 0; i < file_types->extensions.size(); ++i) {
    const std::vector<base::FilePath::StringType>& ext_list =
        file_types->extensions[i];

    // Generate type description for the extension group.
    NSString* type_description = nil;
    if (i < file_types->extension_description_overrides.size() &&
        !file_types->extension_description_overrides[i].empty()) {
      type_description = base::SysUTF16ToNSString(
          file_types->extension_description_overrides[i]);
    } else {
      // No description given for a list of extensions; pick the first one
      // from the list (arbitrarily) and use its description.
      DCHECK(!ext_list.empty());
      type_description = GetDescriptionFromExtension(ext_list[0]);
    }
    DCHECK_NE(0u, [type_description length]);
    [popup addItemWithTitle:type_description];

    // Populate file_type_lists.
    // Set to store different extensions in the current extension group.
    NSMutableArray* file_type_array = [NSMutableArray array];
    for (const base::FilePath::StringType& ext : ext_list) {
      if (ext == default_extension)
        default_extension_index = i;

      // Crash reports suggest that CreateUTIFromExtension may return nil. Hence
      // we nil check before adding to |file_type_set|. See
      // https://crbug.com/630101 and rdar://27490414.
      base::apple::ScopedCFTypeRef<CFStringRef> uti(CreateUTIFromExtension(ext));
      if (uti) {
        NSString* uti_ns = base::apple::CFToNSPtrCast(uti.get());
        if (![file_type_array containsObject:uti_ns])
          [file_type_array addObject:uti_ns];
      }

      // Always allow the extension itself, in case the UTI doesn't map
      // back to the original extension correctly. This occurs with dynamic
      // UTIs on 10.7 and 10.8.
      // See https://crbug.com/148840, https://openradar.appspot.com/12316273
      base::apple::ScopedCFTypeRef<CFStringRef> ext_cf(
          base::SysUTF8ToCFStringRef(ext));
      NSString* ext_ns = base::apple::CFToNSPtrCast(ext_cf.get());
      if (![file_type_array containsObject:ext_ns])
        [file_type_array addObject:ext_ns];
    }
    [file_type_lists addObject:file_type_array];
  }

  if (file_types->include_all_files || file_types->extensions.empty()) {
    panel_.allowsOtherFileTypes = YES;
    // If "all files" is specified for a save panel, allow the user to add an
    // alternate non-suggested extension, but don't add it to the popup. It
    // makes no sense to save as an "all files" file type.
    if (!is_save_panel) {
      [popup addItemWithTitle:l10n_util::GetNSString(IDS_APP_SAVEAS_ALL_FILES)];
    }
  }

  extension_dropdown_handler_ = [[ExtensionDropdownHandler alloc]
      initWithDialog:panel_
       fileTypeLists:file_type_lists];

  // This establishes a weak reference to handler. Hence we persist it as part
  // of dialog_data_list_.
  [popup setTarget:extension_dropdown_handler_];
  [popup setAction:@selector(popupAction:)];

  // file_type_index uses 1 based indexing.
  if (file_type_index) {
    DCHECK_LE(static_cast<size_t>(file_type_index),
              file_types->extensions.size());
    DCHECK_GE(file_type_index, 1);
    [popup selectItemAtIndex:file_type_index - 1];
    [extension_dropdown_handler_ popupAction:popup];
  } else if (!default_extension.empty() && default_extension_index != -1) {
    [popup selectItemAtIndex:default_extension_index];
    [panel_
        setAllowedFileTypes:@[ base::SysUTF8ToNSString(default_extension) ]];
  } else {
    // Select the first item.
    [popup selectItemAtIndex:0];
    [extension_dropdown_handler_ popupAction:popup];
  }

  // There's no need for a popup unless there are at least two choices.
  if (popup.numberOfItems >= 2)
    panel_.accessoryView = accessory_view;
}

void SelectFileDialogBridge::OnPanelEnded(bool did_cancel) {
  if (!show_callback_) {
    return;
  }

  int index = 0;
  std::vector<base::FilePath> paths;
  std::vector<std::string> file_tags;
  if (!did_cancel) {
    if (type_ == SelectFileDialogType::kSaveAsFile) {
      NSURL* url = panel_.URL;
      if (url.isFileURL) {
        paths.push_back(base::apple::NSURLToFilePath(url));
      }

      NSView* accessoryView = panel_.accessoryView;
      if (accessoryView) {
        NSPopUpButton* popup = [accessoryView viewWithTag:kFileTypePopupTag];
        if (popup) {
          // File type indexes are 1-based.
          index = popup.indexOfSelectedItem + 1;
        }
      } else {
        index = 1;
      }

      if (@available(macOS 10.9, *)) {
        // The tag autosetter was turned off when `showsTagField` was set above.
        // Retrieve the tags for assignment later.
        for (NSString* tag in panel_.tagNames) {
          file_tags.push_back(base::SysNSStringToUTF8(tag));
        }
      }
    } else {
      // This does not use ObjCCast because the underlying object could be a
      // non-exported AppKit type (https://crbug.com/41477018).
      NSOpenPanel* open_panel = static_cast<NSOpenPanel*>(panel_);

      for (NSURL* url in open_panel.URLs) {
        if (!url.isFileURL) {
          continue;
        }
        NSString* path = url.path;

        // There is a bug in macOS where, despite a request to disallow file
        // selection, files/packages are able to be selected. If indeed file
        // selection was disallowed, drop any files selected.
        // https://crbug.com/40861123, FB11405008
        if (!open_panel.canChooseFiles) {
          BOOL is_directory;
          BOOL exists =
              [NSFileManager.defaultManager fileExistsAtPath:path
                                                 isDirectory:&is_directory];
          BOOL is_package =
              [NSWorkspace.sharedWorkspace isFilePackageAtPath:path];
          if (!exists || !is_directory || is_package) {
            continue;
          }
        }

        paths.push_back(base::apple::NSStringToFilePath(path));
      }
    }
  }

  std::move(show_callback_).Run(did_cancel, paths, index, file_tags);
}

// static
NSSavePanel* SelectFileDialogBridge::GetLastCreatedNativePanelForTesting() {
  return g_last_created_panel_for_testing;
}

}  // namespace remote_cocoa
