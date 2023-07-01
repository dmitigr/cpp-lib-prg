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
class Info {
public:
  /// The destructor.
  virtual ~Info() = default;

  /// Must be defined in the application!!!
  static std::unique_ptr<Info> make(std::vector<Command> commands);

  /// @returns `true` if instance initialized.
  static bool is_initialized() noexcept
  {
    return static_cast<bool>(instance_);
  }

  /**
   * Initializes the instance.
   *
   * @par Requires
   * `!is_initialized()`.
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
    auto commands = parsed_commands(argc, argv);
    DMITIGR_ASSERT(!commands.empty() && commands[0]);
    instance_ = make(std::move(commands));
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

  /// Non copy-constructible.
  Info(const Info&) = delete;

  /// Non move-constructible.
  Info(Info&&) = delete;

  /// Non copy-assignable.
  Info& operator=(const Info&) = delete;

  /// Non move-assignable.
  Info& operator=(Info&&) = delete;

  /// @returns The vector of commands.
  const std::vector<Command>& commands() const noexcept
  {
    return commands_;
  }

  /// @returns The program name.
  std::string program_name() const
  {
    return std::filesystem::path{commands_[0].name()}.filename().string();
  }

  /// @returns The program synopsis.
  virtual const std::string& synopsis() const noexcept = 0;

  /// The running status of the program.
  std::atomic_bool is_running{false};

protected:
  /// The constructor.
  explicit Info(std::vector<Command> commands)
    : commands_{std::move(commands)}
  {}

private:
  std::vector<Command> commands_;
  inline static std::unique_ptr<Info> instance_;
};

// =============================================================================

/**
 * @brief Prints the usage info on the standard error and terminates the
 * program with unsuccessful exit code.
 *
 * @par Requires
 * `!Info::instance().commands().empty() && Info::instance().commands()[0]`.
 *
 * @param info A formatted information to print.
 */
[[noreturn]] inline void exit_usage(const int code = EXIT_FAILURE,
  std::ostream& out = std::cerr)
{
  const auto& info = Info::instance();
  DMITIGR_ASSERT(!info.commands().empty() && info.commands()[0]);
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
  if (sig == SIGINT)
    Info::instance().is_running = false; // normal shutdown
#ifndef __APPLE__
  else if (sig == SIGTERM)
    std::quick_exit(sig); // abnormal shutdown
#endif
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
  std::filesystem::path working_directory = {},
  std::filesystem::path pid_file = {},
  std::filesystem::path log_file = {},
  const std::ios_base::openmode log_file_mode =
    std::ios_base::trunc | std::ios_base::out)
{
  DMITIGR_ASSERT(startup);
  auto& info = Info::instance();
  DMITIGR_ASSERT(!info.is_running);
  DMITIGR_ASSERT(!info.commands().empty() && info.commands()[0]);

  const auto run = [&startup, &info]
  {
    info.is_running = true;
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
  const fs::path executable{info.commands()[0].name()};
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
    Info::instance().is_running = false; // should cause a normal shutdown
    log::clog() << where << ": " << e.what() << ". Shutting down!\n";
  } catch (...) {
    Info::instance().is_running = false; // should cause a normal shutdown
    log::clog() << where << ": unknown error! Shutting down!\n";
  }
}

} // namespace dmitigr::prg

#endif  // DMITIGR_PRG_RUN_HPP
