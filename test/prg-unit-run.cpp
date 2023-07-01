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

#include "../../prg/run.hpp"

#include <iostream>
#include <utility>

namespace prg = dmitigr::prg;

class My_info : public prg::Info {
public:
  template<typename ... Types>
  My_info(Types&& ... args)
    : prg::Info{std::forward<Types>(args)...}
  {}

  const std::string& synopsis() const noexcept override
  {
    return synopsis_;
  }

private:
  std::string synopsis_{"[--detach]"};
};
std::unique_ptr<prg::Info> prg::Info::make(std::vector<prg::Command> commands)
{
  return std::make_unique<My_info>(std::move(commands));
}

void start()
{
  std::clog << "The service is started!" << std::endl;
  std::clog << "Start flag is " << prg::Info::instance().is_running << std::endl;
}

int main(int argc, char* argv[])
try {
  // Parse and set parameters.
  const auto& info = prg::Info::initialize(argc, argv);
  const auto& cmd = info.commands()[0];

  // Pre-check synopsis.
  if (cmd.options().size() > 1 || !cmd.parameters().empty())
    prg::exit_usage();

  // Check synopsis.
  const auto [detach_o] = cmd.options("detach");
  const bool detach = detach_o.is_valid_throw_if_value();

  // Start the program.
  prg::start(detach, start);
} catch (const std::exception& e) {
  std::cerr << e.what() << std::endl;
  return 1;
} catch (...) {
  std::cerr << "unknown error" << std::endl;
  return 2;
}
