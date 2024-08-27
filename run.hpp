// -*- C++ -*-
//
// Copyright 2023 Dmitry Igrishin
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef DMITIGR_PRG_RUN_HPP
#define DMITIGR_PRG_RUN_HPP

#include "../base/assert.hpp"
#include "../base/noncopymove.hpp"
#include "../fsx/filesystem.hpp"
#include "command.hpp"
#include "detach.hpp"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <exception> // set_terminate()
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace dmitigr::prg {

/// The program info.
class Info : Noncopymove {
public:
  /// The destructor.
  virtual ~Info() = default;

  /// Must be defined in the application!!!
  static std::unique_ptr<Info> make(int argc, const char* const* argv);

  /// @returns `true` if instance initialized.
  static bool is_initialized() noexcept
  {
    return static_cast<bool>(instance_);
  }

  /**
   * Initializes the instance.
   *
   * @par Requires
   * `!is_initialized() && argc && argv`.
   *
   * @par Effects
   * `is_initialized()`.
   *
   * @returns instance().
   *
   * @remarks It makes the most sense to call it from main().
   */
  static Info& initialize(const int argc, const char* const* argv)
  {
    DMITIGR_ASSERT(!instance_);
    DMITIGR_ASSERT(argc);
    DMITIGR_ASSERT(argv);
    instance_ = make(argc, argv);
    return *instance_;
  }

  /*
   * @returns Initialized instance.
   *
   * @par Requires
   * `is_initialized()`.
   */
  static Info& instance() noexcept
  {
    DMITIGR_ASSERT(is_initialized());
    return *instance_;
  }

  /// The stop signal.
  std::atomic_int stop_signal{0};

  /// @returns The program name.
  std::string program_name() const
  {
    return executable_path().filename().string();
  }

  /// @returns The path to the executable.
  virtual const std::filesystem::path& executable_path() const noexcept = 0;

  /// @returns The program synopsis.
  virtual const std::string& synopsis() const noexcept = 0;

private:
  inline static std::unique_ptr<Info> instance_;
};

// =============================================================================

/**
 * @brief Prints the usage info on the standard error and terminates the
 * program with unsuccessful exit code.
 *
 * @par Requires
 * `Info::is_initialized()`.
 */
[[noreturn]] inline void exit_usage(const int code = EXIT_FAILURE,
  std::ostream& out = std::cerr)
{
  const auto& info = Info::instance();
  out << "usage: " << info.program_name();
  if (!info.synopsis().empty())
    out << " " << info.synopsis();
  out << std::endl;
  std::exit(code);
}

// =============================================================================

/// A typical signal handler.
inline void handle_signal(const int sig) noexcept
{
  Info::instance().stop_signal = sig;
}

/**
 * @brief Assigns the `signals` as a signal handler of:
 *   - SIGABRT;
 *   - SIGFPE;
 *   - SIGILL;
 *   - SIGINT;
 *   - SIGSEGV;
 *   - SIGTERM.
 */
inline void set_signals(void(*signals)(int) = &handle_signal) noexcept
{
  std::signal(SIGABRT, signals);
  std::signal(SIGFPE, signals);
  std::signal(SIGILL, signals);
  std::signal(SIGINT, signals);
  std::signal(SIGSEGV, signals);
  std::signal(SIGTERM, signals);
}

/**
 * @brief Assigns the `cleanup` as a handler of:
 *   - std::set_terminate();
 *   - std::at_quick_exit() (not available on macOS);
 *   - std::atexit().
 */
inline void set_cleanup(void(*cleanup)()) noexcept
{
  std::set_terminate(cleanup);
#ifndef __APPLE__
  std::at_quick_exit(cleanup);
#endif
  std::atexit(cleanup);
}

// =============================================================================

/**
 * @brief Calls `startup` in the current process.
 *
 * @param detach Denotes should the process be forked or not.
 * @param startup A function to call. This function is called in a current
 * process if `!detach`, or in a forked process otherwise
 * @param working_directory A path to a new working directory. If not specified
 * the directory of executable is assumed.
 * @param pid_file A path to a PID file. If not specified the name of executable
 * with ".pid" extension in the working directory is assumed.
 * @param log_file A path to a log file. If not specified the name of executable
 * with ".log" extension in the working directory is assumed.
 * @param log_file_mode A file mode for the log file.
 *
 * @par Requires
 * `startup && !Info::instance().is_running && !Info::instance().commands().empty()`.
 */
inline void start(const bool detach,
  void(*startup)(),
  std::filesystem::path executable,
  std::filesystem::path working_directory = {},
  std::filesystem::path pid_file = {},
  std::filesystem::path log_file = {},
  const std::ios_base::openmode log_file_mode =
    std::ios_base::trunc | std::ios_base::out)
{
  DMITIGR_ASSERT(startup);
  DMITIGR_ASSERT(!executable.empty());
  auto& info = Info::instance();
  DMITIGR_ASSERT(!info.stop_signal);

  const auto run = [&startup, &info]
  {
    try {
      startup();
    } catch (const std::exception& e) {
      std::cerr << e.what() << std::endl;
      std::exit(EXIT_FAILURE);
    } catch (...) {
      std::cerr << "unknown error" << std::endl;
      std::exit(EXIT_FAILURE);
    }
  };

  // Preparing.

  namespace fs = std::filesystem;
  if (working_directory.empty())
    working_directory = executable.parent_path();

  if (detach) {
    if (pid_file.empty()) {
      pid_file = working_directory / executable.filename();;
      pid_file.replace_extension(".pid");
    }
    if (log_file.empty()) {
      log_file = working_directory / executable.filename();
      log_file.replace_extension(".log");
    }
    fs::create_directories(pid_file.parent_path());
    fs::create_directories(log_file.parent_path());
  }

  // Starting.

  log::is_clog_with_now = detach;
  if (!detach) {
    DMITIGR_ASSERT(!working_directory.empty());
    std::error_code errc;
    std::filesystem::current_path(working_directory, errc);
    if (errc) {
      std::cerr << "cannot change the working directory to "
                << working_directory.string() + ": " + errc.message() << std::endl;
      std::exit(EXIT_FAILURE);
    }

    if (!pid_file.empty())
      log::dump_pid(pid_file);

    if (!log_file.empty())
      log::redirect_clog(log_file, log_file_mode);

    run();
  } else
    prg::detach(run, working_directory, pid_file, log_file, log_file_mode);
}

// =============================================================================

/**
 * @brief Calls the function `f`.
 *
 * @details If the call of `callback` fails with exception then
 * `Info::instance().is_running` flag is sets to `false` which should
 * cause the normal application shutdown.
 *
 * @param f A function to call
 * @param where A descriptive context of call for printing to `std::clog`.
 */
template<typename F>
auto with_shutdown_on_error(F&& f, const std::string_view where) noexcept
{
  try {
    return f();
  } catch (const std::exception& e) {
    Info::instance().stop_signal = SIGTERM; // should cause a normal shutdown
    log::clog() << where << ": " << e.what() << ". Shutting down!\n";
  } catch (...) {
    Info::instance().stop_signal = SIGTERM; // should cause a normal shutdown
    log::clog() << where << ": unknown error! Shutting down!\n";
  }
}

} // namespace dmitigr::prg

#endif  // DMITIGR_PRG_RUN_HPP
