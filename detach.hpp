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

#ifdef _WIN32
#error prg/detach.hpp is not usable on Microsoft Windows!
#endif

#ifndef DMITIGR_PRG_DETACH_HPP
#define DMITIGR_PRG_DETACH_HPP

#include "../base/assert.hpp"
#include "../fsx/filesystem.hpp"
#include "../log/log.hpp"
#include "../os/error.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <functional>

#include <sys/stat.h>
#include <sys/types.h>

namespace dmitigr::prg {

/**
 * @brief Detaches the process to make it work in background.
 *
 * @param startup The startup function to be called from a forked process.
 * @param pid_file The PID file that will be created and to which the
 * ID of the forked process will be written.
 * @param log_file The log file the detached process will use as the
 * destination instead of `std::clog` to write the log info.
 * @param log_file_openmode The openmode to use upon opening the specified
 * `log_file`.
 *
 * @par Requires
 * `(startup && !working_directory.empty() && !pid_file.empty() && !log_file.empty)`.
 *
 * @remarks The function returns in the detached (forked) process!
 */
inline void detach(const std::function<void()>& startup,
  const std::filesystem::path& working_directory,
  const std::filesystem::path& pid_file,
  const std::filesystem::path& log_file,
  const std::ios_base::openmode log_file_openmode =
  std::ios_base::app | std::ios_base::ate | std::ios_base::out)
{
  DMITIGR_ASSERT(startup);
  if (working_directory.empty()) {
    log::clog() << "cannot detach process because the working directory isn't "
                << "specified\n";
    std::exit(EXIT_SUCCESS);
  }
  if (pid_file.empty() || pid_file == "." || pid_file == "..") {
    log::clog() << "cannot detach process because the PID file name is invalid\n";
    std::exit(EXIT_SUCCESS);
  }
  if (log_file.empty() || log_file == "." || log_file == "..") {
    log::clog() << "cannot detach process because the log file name is invalid\n";
    std::exit(EXIT_SUCCESS);
  }

  // Forking for a first time
  if (const auto pid = ::fork(); pid < 0) {
    const int err = errno;
    log::clog() << "first fork() failed (" << os::error_message(err) << ")\n";
    std::exit(EXIT_FAILURE); // exit parent
  } else if (pid > 0)
    std::exit(EXIT_SUCCESS); // exit parent

  // Setting the umask for a new child process
  ::umask(S_IWGRP | S_IRWXO);

  // Redirecting clog to `log_file`.
  try {
    log::redirect_clog(log_file, log_file_openmode);
  } catch (const std::exception& e) {
    log::clog() << e.what() << '\n';
    std::exit(EXIT_FAILURE); // exit parent
  } catch (...) {
    log::clog() << "cannot redirect std::clog to " << log_file << '\n';
    std::exit(EXIT_FAILURE); // exit parent
  }

  // Setup the new process group leader
  if (const auto sid = ::setsid(); sid < 0) {
    const int err = errno;
    log::clog() << "cannot setup the new process group leader ("
                << os::error_message(err) << ")\n";
    std::exit(EXIT_FAILURE);
  }

  // Forking for a second time
  if (const auto pid = ::fork(); pid < 0) {
    const int err = errno;
    log::clog() << "second fork() failed (" << os::error_message(err) << ")\n";
    std::exit(EXIT_FAILURE);
  } else if (pid > 0)
    std::exit(EXIT_SUCCESS);

  // Creating the PID file
  try {
    log::dump_pid(pid_file);
  } catch (const std::exception& e) {
    log::clog() << e.what() << '\n';
    std::exit(EXIT_FAILURE);
  } catch (...) {
    log::clog() << "cannot open log file at " << log_file << '\n';
    std::exit(EXIT_FAILURE);
  }

  // Changing the CWD
  try {
    std::filesystem::current_path(working_directory);
  } catch (const std::exception& e) {
    log::clog() << e.what() << '\n';
    std::exit(EXIT_FAILURE);
  } catch (...) {
    log::clog() << "cannot change current working directory to "
                << working_directory << '\n';
    std::exit(EXIT_FAILURE);
  }

  // Closing the standard file descriptors
  static auto close_fd = [](const int fd)
  {
    if (::close(fd)) {
      const int err = errno;
      log::clog() << "cannot close file descriptor " << fd
                  << " (" << os::error_message(err) << ")\n";
      std::exit(EXIT_FAILURE);
    }
  };

  close_fd(STDIN_FILENO);
  close_fd(STDOUT_FILENO);
  close_fd(STDERR_FILENO);

  // Starting up.
  try {
    startup();
  } catch (const std::exception& e) {
    log::clog() << e.what() << '\n';
    std::exit(EXIT_FAILURE);
  } catch (...) {
    log::clog() << "start routine failed" << '\n';
    std::exit(EXIT_FAILURE);
  }
}

} // namespace dmitigr::prg

#endif  // DMITIGR_PRG_DETACH_HPP
