// Windows and Linux implementations of FileDialog (macOS is in File_Dialog_mac.mm).

#include "File_Dialog.h"

#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>

namespace FileDialog {
    namespace {
        std::wstring Widen(const std::string& s)
        {
            if (s.empty()) return {};
            const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
            std::wstring w(n, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
            w.resize(n - 1);
            return w;
        }

        std::string Narrow(const wchar_t* w)
        {
            const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
            std::string s(n, '\0');
            WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
            s.resize(n - 1);
            return s;
        }
    }

    Result Save(const char* title, const char* default_name, const char* extension, std::string& path)
    {
        wchar_t file[MAX_PATH] = {};
        const std::wstring name = Widen(default_name);
        wcsncpy_s(file, name.c_str(), MAX_PATH - 1);

        const std::wstring ext = Widen(extension);
        // Filter: "JSON files (*.json)\0*.json\0All files\0*.*\0\0"
        std::wstring filter = Widen(std::string(extension)) + L" files (*." + ext + L")";
        filter.push_back(L'\0');
        filter += L"*." + ext;
        filter.push_back(L'\0');
        filter += L"All files";
        filter.push_back(L'\0');
        filter += L"*.*";
        filter.push_back(L'\0');
        filter.push_back(L'\0');
        const std::wstring wtitle = Widen(title);

        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = GetActiveWindow();
        ofn.lpstrFilter = filter.c_str();
        ofn.lpstrFile = file;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = wtitle.c_str();
        ofn.lpstrDefExt = ext.c_str();
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetSaveFileNameW(&ofn))
            return Result::Cancelled;
        path = Narrow(file);
        return Result::Ok;
    }
}

#elif !defined(__APPLE__)

#include <cstdio>
#include <cstdlib>

namespace FileDialog {
    namespace {
        bool HasCommand(const char* name)
        {
            const std::string cmd = std::string("command -v ") + name + " >/dev/null 2>&1";
            return std::system(cmd.c_str()) == 0;
        }

        // Single-quote for /bin/sh
        std::string Quote(const std::string& s)
        {
            std::string q = "'";
            for (char c : s) {
                if (c == '\'') q += "'\\''";
                else q += c;
            }
            return q + "'";
        }

        bool Run(const std::string& cmd, std::string& out)
        {
            FILE* p = popen(cmd.c_str(), "r");
            if (!p) return false;
            char buf[4096];
            out.clear();
            while (fgets(buf, sizeof(buf), p)) out += buf;
            const int status = pclose(p);
            while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
            return status == 0 && !out.empty();
        }
    }

    Result Save(const char* title, const char* default_name, const char* extension, std::string& path)
    {
        const char* home = std::getenv("HOME");
        const std::string start = std::string(home ? home : ".") + "/" + default_name;
        std::string out;

        if (HasCommand("zenity")) {
            const std::string cmd = "zenity --file-selection --save --confirm-overwrite --title=" + Quote(title)
                                  + " --filename=" + Quote(start) + " 2>/dev/null";
            if (!Run(cmd, out)) return Result::Cancelled;
        }
        else if (HasCommand("kdialog")) {
            const std::string cmd = "kdialog --title " + Quote(title) + " --getsavefilename " + Quote(start)
                                  + " " + Quote(std::string("*.") + extension) + " 2>/dev/null";
            if (!Run(cmd, out)) return Result::Cancelled;
        }
        else {
            path = "No file dialog available: install zenity (GNOME) or kdialog (KDE)";
            return Result::Unavailable;
        }

        path = out;
        const size_t slash = path.find_last_of('/');
        if (path.find('.', slash == std::string::npos ? 0 : slash + 1) == std::string::npos)
            path += std::string(".") + extension;
        return Result::Ok;
    }
}

#endif
