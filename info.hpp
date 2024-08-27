// -*- C++ -*-
//
// Copyright 2024 Dmitry Igrishin
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

#ifndef DMITIGR_PRG_INFO_HPP
#define DMITIGR_PRG_INFO_HPP

#include "../base/assert.hpp"
#include "../base/noncopymove.hpp"
#include "../fsx/filesystem.hpp"

#include <atomic>
#include <memory>
#include <string>

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

} // namespace dmitigr::prg

#endif  // DMITIGR_PRG_INFO_HPP