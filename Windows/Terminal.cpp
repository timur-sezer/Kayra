#include "Terminal.h"

#include "Theme.h"
#include "imgui.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#define KAYRA_PIPE(fds)  _pipe(fds, 65536, _O_BINARY)
#define KAYRA_DUP        _dup
#define KAYRA_DUP2       _dup2
#define KAYRA_READ       _read
#define KAYRA_WRITE      _write
#define KAYRA_CLOSE      _close
#else
#include <unistd.h>
#define KAYRA_PIPE(fds)  pipe(fds)
#define KAYRA_DUP        dup
#define KAYRA_DUP2       dup2
#define KAYRA_READ       read
#define KAYRA_WRITE      write
#define KAYRA_CLOSE      close
#endif

namespace Terminal {
    namespace {
        enum Level { Debug = 0, Info, Warn, Error };

        struct Line {
            Level level;        // only used for the text color
            std::string text;   // exactly as printed (color codes removed)
        };

        constexpr size_t kMaxLines = 5000;

        std::mutex        g_mutex;
        std::deque<Line>  g_lines;
        uint64_t          g_total = 0;      // lines ever added (lets views detect new output)

        std::thread       g_reader;
        std::atomic<bool> g_capturing{false};
        int g_pipe_read = -1, g_pipe_write = -1;
        int g_saved_stdout = -1, g_saved_stderr = -1;

        // Removes ANSI color codes (MAVSDK colors its log levels).
        std::string StripAnsi(const std::string& s)
        {
            std::string out;
            out.reserve(s.size());
            for (size_t i = 0; i < s.size(); ++i) {
                if (s[i] == '\x1b' && i + 1 < s.size() && s[i + 1] == '[') {
                    i += 2;
                    while (i < s.size() && !(s[i] >= '@' && s[i] <= '~')) ++i;
                    continue;
                }
                if (s[i] != '\r') out.push_back(s[i]);
            }
            return out;
        }

        Level Classify(const std::string& s)
        {
            // MAVSDK: "[09:10:59|Info ] message (file.cpp:42)"
            if (s.find("|Debug]") != std::string::npos) return Debug;
            if (s.find("|Info ]") != std::string::npos) return Info;
            if (s.find("|Warn ]") != std::string::npos) return Warn;
            if (s.find("|Error]") != std::string::npos) return Error;
            std::string lower = s;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            if (lower.find("error") != std::string::npos || lower.find("failed") != std::string::npos
                || lower.find("could not") != std::string::npos)
                return Error;
            if (lower.find("warn") != std::string::npos || lower.find("lost") != std::string::npos)
                return Warn;
            return Info;
        }

        void AddLine(const std::string& raw)
        {
            std::string text = StripAnsi(raw);
            if (text.empty())
                return;
            Line line;
            line.level = Classify(text);
            line.text = std::move(text);

            std::lock_guard<std::mutex> lock(g_mutex);
            g_lines.push_back(std::move(line));
            if (g_lines.size() > kMaxLines)
                g_lines.pop_front();
            ++g_total;
        }

        void ReaderLoop()
        {
            char buf[4096];
            std::string pending;
            while (true) {
                const int n = int(KAYRA_READ(g_pipe_read, buf, sizeof(buf)));
                if (n <= 0)
                    break;  // write end closed: capture stopped
                if (g_saved_stdout >= 0)
                    (void)KAYRA_WRITE(g_saved_stdout, buf, unsigned(n));  // keep the real terminal working
                pending.append(buf, size_t(n));
                size_t nl;
                while ((nl = pending.find('\n')) != std::string::npos) {
                    AddLine(pending.substr(0, nl));
                    pending.erase(0, nl + 1);
                }
            }
            if (!pending.empty())
                AddLine(pending);
        }

        // Stops capturing at program exit if main() returned early without StopCapture()
        // (an unjoined std::thread would abort the program). Defined after g_reader, so it
        // is destroyed first.
        struct ExitGuard { ~ExitGuard(); } g_exit_guard;

    }

    void StartCapture()
    {
        if (g_capturing)
            return;
        int fds[2];
        if (KAYRA_PIPE(fds) != 0)
            return;
        g_pipe_read = fds[0];
        g_pipe_write = fds[1];

        fflush(stdout);
        fflush(stderr);
        g_saved_stdout = KAYRA_DUP(1);
        g_saved_stderr = KAYRA_DUP(2);
        KAYRA_DUP2(g_pipe_write, 1);
        KAYRA_DUP2(g_pipe_write, 2);
        // A pipe makes stdout fully buffered; line buffering shows messages as they happen.
        setvbuf(stdout, nullptr, _IOLBF, 0);
        setvbuf(stderr, nullptr, _IONBF, 0);

        g_capturing = true;
        g_reader = std::thread(ReaderLoop);
    }

    void StopCapture()
    {
        if (!g_capturing)
            return;
        fflush(stdout);
        fflush(stderr);
        // Point stdout/stderr back at the terminal; closing the last write end ends the reader.
        KAYRA_DUP2(g_saved_stdout, 1);
        KAYRA_DUP2(g_saved_stderr, 2);
        KAYRA_CLOSE(g_pipe_write);
        g_pipe_write = -1;
        if (g_reader.joinable())
            g_reader.join();
        KAYRA_CLOSE(g_pipe_read);
        KAYRA_CLOSE(g_saved_stdout);
        KAYRA_CLOSE(g_saved_stderr);
        g_pipe_read = g_saved_stdout = g_saved_stderr = -1;
        g_capturing = false;
    }

    namespace {
        ExitGuard::~ExitGuard() { StopCapture(); }
    }

    void ShowWindow(const char* title, bool* p_open)
    {
        if (!*p_open)
            return;

        // Just the output, like a terminal: no toolbar, the window itself scrolls.
        const Theme::Palette& p = Theme::P();
        ImGui::PushStyleColor(ImGuiCol_WindowBg, p.bg);
        const bool open = ImGui::Begin(title, p_open, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::PopStyleColor();
        if (!open) {
            ImGui::End();
            return;
        }

        ImGui::PushFont(Theme::Get(Theme::Font::Small));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 2.0f * Theme::Scale()));
        {
            std::lock_guard<std::mutex> lock(g_mutex);  // the reader thread only appends
            ImGuiListClipper clipper;
            clipper.Begin(int(g_lines.size()));
            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    const Line& l = g_lines[size_t(i)];
                    const ImVec4 col = l.level == Error ? p.bad
                                     : l.level == Warn  ? p.warn
                                     : l.level == Debug ? p.text_dim
                                                        : p.text;
                    ImGui::PushStyleColor(ImGuiCol_Text, col);
                    ImGui::TextUnformatted(l.text.c_str());
                    ImGui::PopStyleColor();
                }
            }
            clipper.End();
        }

        // Follow new output while scrolled to the bottom; scrolling up stops following.
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
            ImGui::SetScrollHereY(1.0f);

        ImGui::PopStyleVar();
        ImGui::PopFont();
        ImGui::End();
    }
}
