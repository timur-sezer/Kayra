#pragma once

// Native "Save As" dialog.
//   macOS: NSSavePanel (File_Dialog_mac.mm)   Windows: GetSaveFileNameW   Linux: zenity or kdialog

#include <string>

namespace FileDialog {
    enum class Result { Ok, Cancelled, Unavailable };

    // Blocks until the user picks a file or cancels. `default_name` is prefilled (e.g. "mission.json");
    // `extension` (without dot) is appended if the user omits it. On Ok, `path` holds the chosen file.
    // Unavailable means no dialog could be shown; `path` then holds a reason.
    Result Save(const char* title, const char* default_name, const char* extension, std::string& path);
}
