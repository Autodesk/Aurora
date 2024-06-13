// Copyright 2023 Autodesk, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <vector>

// undefine snprintf (which is defined to _snprintf on some systems)
#ifdef snprintf
#undef snprintf
#endif

// Event logging is support by a set of macros defined here, all of which have the "AU_" prefix.
// The Log class and associated singleton object can be used to configure logging behavior. The log
// is not for providing messages to end users; any messages are intended only for other developers.
// The log macros and their intended use are described below and in the coding guidelines:
//
// AU_INFO: Use this to provide diagnostic information to developers. For example, progress reports
// or the results of successful file operations.
//
// AU_WARN: Use this to indicate to developers that a requested operation succeeded, but that some
// undesirable result may have occurred. For example, a resource exceeding a recommended size in
// memory.
//
// AU_ERROR: Use this to indicate to developers that a requested operation could not be completed
// as requested. For example, a requested data entry or file was not found and therefore could not
// be processed. This is usually followed by some form of error handling.
//
// AU_FAIL: In addition to reporting a message, this also aborts the application. Use this to
// indicate a catastrophic failure, due to an unhandled programming error. For example, a switch
// statement reaches an unexpected case. User action should not intentionally result in a failure.
//
// AU_ASSERT: This accepts a test condition that is expected to be true. If the condition evaluates
// to false, the failure is reported and the application is immediately aborted as with AU_FAIL. By
// convention, the test condition must not produce side effects. Use this throughout the code to
// ensure expected conditions are met, with any failures being the result of unhandled programming
// errors.
//
// AU_ASSERT_DEBUG: This is identical to AU_ASSERT except that it only executes in debug
// builds. Use this in performance-critical sections of the code, where it would be undesirable to
// test conditions in release builds. This should be used rarely, preferring AU_ASSERT.
//
// AU_DEBUG_BREAK: Use this to break into the attached debugger, if any. This only works in debug
// builds.

// Set the ADD_FILE_AND_LINE_TO_LOGS symbol to 0 to remove file and line information from log
// output.
#if !defined(ADD_FILE_AND_LINE_TO_LOGS)
#define ADD_FILE_AND_LINE_TO_LOGS 1
#endif

// Only include file and line information in log output if ADD_FILE_AND_LINE_TO_LOGS is set
// to 1.
#if ADD_FILE_AND_LINE_TO_LOGS
#define AU_FILE __FILE__
#define AU_LINE __LINE__
#else
#define AU_FILE ""
#define AU_LINE 0
#endif

// This is to fix "ISO C++11 requires at least one argument for the "..." in a variadic macro".
#pragma GCC system_header

/// Triggers an information event, which logs a message along with the current file and line.
#define AU_INFO(_msg, ...) Aurora::Foundation::Log::info(AU_FILE, AU_LINE, _msg, ##__VA_ARGS__)

/// Triggers a warning event, which logs a message along with the current file and line.
#define AU_WARN(_msg, ...) Aurora::Foundation::Log::warn(AU_FILE, AU_LINE, _msg, ##__VA_ARGS__)

/// Triggers an error event, which logs a message along with the current file and line.
#define AU_ERROR(_msg, ...) Aurora::Foundation::Log::error(AU_FILE, AU_LINE, _msg, ##__VA_ARGS__)

/// Triggers a failure event, which logs a message along with the current file and line.
///
/// This is considered a catastrophic failure and will abort the application.
#define AU_FAIL(_msg, ...) Aurora::Foundation::Log::fail(AU_FILE, AU_LINE, _msg, ##__VA_ARGS__)

/// Tests the _check condition and triggers a failure event if it is false.
#define AU_ASSERT(_check, _msg, ...)                                                               \
    do                                                                                             \
    {                                                                                              \
        if (!(_check))                                                                             \
        {                                                                                          \
            Aurora::Foundation::Log::fail(AU_FILE, AU_LINE,                                        \
                "AU_ASSERT test failed:\nEXPRESSION: %s\nDETAILS: " _msg, #_check, ##__VA_ARGS__); \
        }                                                                                          \
    } while (false)

/// Tests the _check condition and triggers a failure event if it is false.
///
/// This is completely removed in non-debug builds, so this is intended for checks in
/// performance sensitive situations.
#if defined NDEBUG
#define AU_ASSERT_DEBUG(_check, _msg, ...)                                                         \
    do                                                                                             \
    {                                                                                              \
    } while (false)
#else
#define AU_ASSERT_DEBUG(_check, _msg, ...) AU_ASSERT(_check, _msg, ##__VA_ARGS__)
#endif

/// Breaks into the attached debugger, if any.
///
/// This does nothing in non-debug builds.
#define AU_DEBUG_BREAK() Aurora::Foundation::Log::debugBreak();

namespace Aurora
{
namespace Foundation
{

/// A class which manages event logging.
class Log
{
public:
    /*** Types ***/

    /// An enumeration of event levels.
    ///
    /// Setting a specific level means all higher levels are included, e.g. setting kError
    /// means error and failure (kFail) events are logged.
    enum class Level
    {
        kInfo = 0, // information events
        kWarn,     // warning events
        kError,    // error events
        kFail,     // failure events, including assertion failures
        kNone      // disable logging
    };

    /// A callback function type for custom logging functionality.
    ///
    /// The function accepts the file and line number where the event occurred, as well as
    /// the event level and message.
    using CBFunction =
        std::function<bool(const std::string& file, int line, Level level, const std::string& msg)>;

    /*** Functions ***/

    /// Sets a custom logging callback function.
    void setLogFunction(CBFunction cb) { _logCB = cb; }

    /// Sets the minimum log level.
    //
    /// Events with this level or higher will be logged.
    void setLogLevel(Level level) { _logLevel = level; }

    /// Enables displaying a dialog box on failure events.
    void enableFailureDialog(bool enabled) { _failureDialogEnabled = enabled; }

    /// Performs a catastrophic abort, with an optional failure dialog to cancel the abort.
    template <typename... Args>
    void abort(const std::string& file, int line, const std::string& format, Args... args)
    {
        // Abort the application if _failureMessageBoxEnabled is false or displayFailureDialog()
        // returns true.
        if (!_failureDialogEnabled ||
            displayFailureDialog(file, line, stringFormat(format, args...)))
        {
            ::abort();
        }
    }

    /// Logs event information, as used by the other log functions.
    template <typename... Args>
    bool log(Level messageLevel, std::ostream& stream, const std::string& file, int line,
        const std::string& format, Args... args)
    {
        // Do nothing if the current log level is higher than level for this message.
        if (_logLevel > messageLevel)
        {
            return false;
        }

        // Format the message with the variable arguments.
        std::string formattedMsg = stringFormat(format, args...);

        // Do nothing if the log callback returns false.
        if (_logCB && !_logCB(file, line, messageLevel, formattedMsg))
        {
            return false;
        }

        // Get the file and line prefix for non-zero lines and the expanded format string.
        std::string prefix = line > 0 ? (file + " (" + std::to_string(line) + "):\t") : "";
        std::string msg    = prefix + formattedMsg;

        // Output to the debug console and the specified output stream.
        writeToConsole(msg);
        stream << msg;

        return true;
    }

    /*** Static Functions ***/

    /// Gets the singleton logger for the application.
    static Log& logger();

    /// Logs information event output from the application.
    ///
    /// \note Most code should use the AU_INFO macro, instead of calling this directly.
    template <typename... Args>
    static bool info(const std::string& file, int line, const std::string& format, Args... args)
    {
        return logger().log(Level::kInfo, std::cout, file, line, format, args...);
    }

    /// Logs warning event output from the application.
    ///
    /// \note Most code should use the AU_WARN macro, instead of calling this directly.
    template <typename... Args>
    static bool warn(const std::string& file, int line, const std::string& format, Args... args)
    {
        return logger().log(Level::kWarn, std::cerr, file, line, format, args...);
    }

    /// Logs error event output from the application.
    ///
    /// \note Most code should use the AU_ERROR macro, instead of calling this directly.
    template <typename... Args>
    static bool error(const std::string& file, int line, const std::string& format, Args... args)
    {
        return logger().log(Level::kError, std::cerr, file, line, format, args...);
    }

    /// Logs failure event output and terminates the application.
    ///
    /// \note Most code should use the AU_FAIL macro, instead of calling this directly.
    template <typename... Args>
    static bool fail(const std::string& file, int line, const std::string& format, Args... args)
    {
        if (logger().log(Level::kFail, std::cerr, file, line, format, args...))
        {
            logger().abort(file, line, format, args...);

            return true;
        }

        return false;
    }

    /// Breaks into the attached debugger, if any.
    ///
    /// This does nothing in non-debug builds.
    static void debugBreak();

    // Writes a string to the console.
    static void writeToConsole(const std::string& msg);

private:
    /*** Private Functions ***/

    /// Creates a string from variable arguments.
    template <typename... Args>
    std::string stringFormat(const std::string& format, Args... args)
    {
        // Disable the Clang security warning.
        // TODO: Implement a better workaround for this warning.
#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
#endif
        // Calculate the size of the expanded format string.
        const char* formatStr = format.c_str();
        int size              = std::snprintf(nullptr, 0, formatStr, args...) + 2;
        if (size <= 0)
        {
            // Report an error if formating fails.
            std::cerr << __FUNCTION__ << " Error during formatting." << std::endl;

            return "";
        }

        // Allocate a buffer for the formatted string and place the expanded string in it.
        std::vector<char> buf(size);
        std::snprintf(buf.data(), size, formatStr, args...);
#if __clang__
#pragma clang diagnostic pop
#endif

        // Add a newline if there is not already one at the end, and terminate the string.
        size_t offset = static_cast<size_t>(size);
        if (buf[offset - 3] != '\n')
        {
            buf[offset - 2] = '\n';
            buf[offset - 1] = 0;
        }
        else
        {
            buf[offset - 2] = 0;
        }

        // Return a new string object from the buffer.
        // TODO: Avoid multiple allocations.
        return std::string(buf.data(), buf.data() + size - 1);
    }

    // Displays a failure dialog box.
    bool displayFailureDialog(const std::string& file, int line, const std::string& msg);

    /*** Private Variables ***/

    // The current log callback.
    CBFunction _logCB;

    // The current log level.
    Level _logLevel = Level::kInfo;

    // Whether to display a dialog box on failure events.
    // NOTE: Defaults to true on debug builds, false otherwise.
#if defined NDEBUG
    bool _failureDialogEnabled = false;
#else
    bool _failureDialogEnabled = true;
#endif
};

} // namespace Foundation
} // namespace Aurora
