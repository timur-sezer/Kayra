// macOS implementation of FileDialog (see File_Dialog.h).

#include "File_Dialog.h"

#import <AppKit/AppKit.h>

namespace FileDialog {
    Result Save(const char* title, const char* default_name, const char* extension, std::string& path)
    {
        @autoreleasepool {
            // Remember which window had focus (the GLFW window) to give it back afterwards.
            NSWindow* previous = [NSApp keyWindow];

            NSSavePanel* panel = [NSSavePanel savePanel];
            panel.title = [NSString stringWithUTF8String:title];
            panel.nameFieldStringValue = [NSString stringWithUTF8String:default_name];
            panel.canCreateDirectories = YES;
            panel.showsTagField = NO;
            panel.extensionHidden = NO;

            const NSModalResponse response = [panel runModal];
            [previous makeKeyAndOrderFront:nil];

            if (response != NSModalResponseOK || panel.URL == nil)
                return Result::Cancelled;

            path = panel.URL.path.UTF8String;
            const std::string ext = std::string(".") + extension;
            if (path.size() < ext.size() || path.compare(path.size() - ext.size(), ext.size(), ext) != 0) {
                // Keep a user-typed extension such as ".plan"; only add one when there is none.
                const size_t slash = path.find_last_of('/');
                if (path.find('.', slash == std::string::npos ? 0 : slash + 1) == std::string::npos)
                    path += ext;
            }
            return Result::Ok;
        }
    }
}
