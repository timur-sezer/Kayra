#pragma once

// In-app terminal: everything the program writes to stdout/stderr (KAYRA's own messages,
// MAVSDK's log, map tile errors) is captured through a pipe, kept in a ring buffer and shown
// in a "Terminal" window (output only, with a scrollbar). Output is still echoed to the real terminal.

namespace Terminal {
    // Window titles: the same log in the Flight and Mission Plan tabs (each tab has its own window).
    inline constexpr const char* kFlightTitle = "Terminal###FlightTerminal";
    inline constexpr const char* kPlanTitle   = "Terminal###PlanTerminal";

    void StartCapture();   // call first thing in main(), before anything prints
    void StopCapture();    // restores stdout/stderr; call last

    void ShowWindow(const char* title, bool* p_open);
}
