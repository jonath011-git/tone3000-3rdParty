#pragma once

// Best-effort crash diagnostics for the standalone app and plugin.
// Logs and Windows minidumps are written under the user's TONE3000 log folder.
namespace CrashDiagnostics {
void install();
void logEvent(const char* area, const char* message);
}
