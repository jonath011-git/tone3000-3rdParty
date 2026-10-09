#include "CrashDiagnostics.h"

#include <juce_core/juce_core.h>

#include <exception>
#include <cstdlib>
#include <mutex>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <DbgHelp.h>
#endif

namespace {
std::mutex diagnosticsMutex;

juce::File getDiagnosticsDirectory() {
  auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                 .getChildFile("TONE3000")
                 .getChildFile("CrashReports");
  if (!dir.exists())
    dir.createDirectory();
  return dir;
}

juce::File getCrashLogFile() {
  return getDiagnosticsDirectory().getChildFile("diagnostics.log");
}

void appendLine(const juce::String& line) noexcept {
  try {
    const std::lock_guard<std::mutex> lock(diagnosticsMutex);
    auto file = getCrashLogFile();
    file.appendText(juce::Time::getCurrentTime().toISO8601(true) + " " + line + "\n",
                    false, false, "\n");
  } catch (...) {
    // Diagnostics must never create a second failure.
  }
}

#if JUCE_WINDOWS
LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo) {
  const auto now = juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S");
  auto dumpFile = getDiagnosticsDirectory().getChildFile("TONE3000-crash-" + now + ".dmp");
  HANDLE fileHandle = CreateFileW(dumpFile.getFullPathName().toWideCharPointer(),
                                  GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
  bool dumpWritten = false;
  if (fileHandle != INVALID_HANDLE_VALUE) {
    MINIDUMP_EXCEPTION_INFORMATION info{};
    info.ThreadId = GetCurrentThreadId();
    info.ExceptionPointers = exceptionInfo;
    info.ClientPointers = FALSE;
    dumpWritten = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                                    fileHandle, MiniDumpWithThreadInfo,
                                    exceptionInfo != nullptr ? &info : nullptr,
                                    nullptr, nullptr) == TRUE;
    CloseHandle(fileHandle);
    if (!dumpWritten)
      dumpFile.deleteFile();
  }

  const auto code = exceptionInfo != nullptr && exceptionInfo->ExceptionRecord != nullptr
                        ? (juce::String::toHexString(
                              static_cast<juce::int64>(exceptionInfo->ExceptionRecord->ExceptionCode)))
                        : juce::String("unknown");
  appendLine("[CRASH] Unhandled Windows exception code=0x" + code
             + " | dump=" + (dumpWritten ? dumpFile.getFullPathName() : juce::String("FAILED")));
  return EXCEPTION_EXECUTE_HANDLER;
}
#endif

void terminateHandler() noexcept {
  appendLine("[FATAL] std::terminate invoked (uncaught C++ exception or termination).");
#if JUCE_WINDOWS
  // Force a normal unhandled exception so the installed filter can collect a dump.
  RaiseException(0xE0000001, EXCEPTION_NONCONTINUABLE, 0, nullptr);
#endif
  std::abort();
}
}  // namespace

namespace CrashDiagnostics {
void install() {
  static std::once_flag installed;
  std::call_once(installed, [] {
    appendLine("[START] Crash diagnostics installed. PID="
#if JUCE_WINDOWS
               + juce::String(static_cast<juce::int64>(GetCurrentProcessId()))
#else
               + juce::String("unknown")
#endif
    );
    std::set_terminate(terminateHandler);
#if JUCE_WINDOWS
    SetUnhandledExceptionFilter(unhandledExceptionFilter);
#endif
  });
}

void logEvent(const char* area, const char* message) {
  appendLine("[" + juce::String(area != nullptr ? area : "GENERAL") + "] "
             + juce::String(message != nullptr ? message : ""));
}
}  // namespace CrashDiagnostics
