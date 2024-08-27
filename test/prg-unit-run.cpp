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

#include "../../prg/command.hpp"
#include "../../prg/info.hpp"
#include "../../prg/util.hpp"

#include <iostream>
#include <utility>

namespace prg = dmitigr::prg;

class My_info : public prg::Info {
public:
  /// @returns The vector of commands.
  const std::vector<prg::Command>& commands() const noexcept
  {
    return commands_;
  }

  static My_info& instance() noexcept
  {
    return static_cast<My_info&>(Info::instance());
  }

  const std::filesystem::path& executable_path() const noexcept override
  {
    return executable_path_;
  }

  const std::string& synopsis() const noexcept override
  {
    return synopsis_;
  }

private:
  friend std::unique_ptr<Info> prg::Info::make(const int, const char* const*);
  std::filesystem::path executable_path_;
  std::string synopsis_{};
  std::vector<prg::Command> commands_;
};
std::unique_ptr<prg::Info> prg::Info::make(const int argc, const char* const* argv)
{
  auto result = std::make_unique<My_info>();
  result->executable_path_ = canonical(std::filesystem::path{argv[0]});
  result->commands_ = prg::parsed_commands(argc, argv);
  result->synopsis_ = "[--detach]";
  DMITIGR_ASSERT(!result->commands_.empty() && result->commands_[0]);
  return result;
}

int main(int argc, char* argv[])
try {
  // Parse and set parameters.
  prg::Info::initialize(argc, argv);
  const auto& info = My_info::instance();
  const auto& cmd = info.commands()[0];

  // Pre-check synopsis.
  if (cmd.options().size() > 1 || !cmd.parameters().empty())
    prg::exit_usage();

  // Check synopsis.
  const auto [detach_o] = cmd.options("detach");
  detach_o.is_valid_throw_if_value();
} catch (const std::exception& e) {
  std::cerr << e.what() << std::endl;
  return 1;
} catch (...) {
  std::cerr << "unknown error" << std::endl;
  return 2;
}
