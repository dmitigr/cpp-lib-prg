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

#ifndef DMITIGR_PRG_COMMAND_HPP
#define DMITIGR_PRG_COMMAND_HPP

#include "../base/assert.hpp"
#include "exceptions.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace dmitigr::prg {

/**
 * @brief A command.
 *
 * @details Stores the parsed command with it's options and parameters.
 */
class Command final {
public:
  /// The alias to represent a map of command options.
  using Option_map = std::map<std::string, std::optional<std::string>>;

  /// The alias to represent a vector of command parameters.
  using Parameter_vector = std::vector<std::string>;

  /**
   * @brief An option reference.
   *
   * @warning The lifetime of the instances of this class is limited by
   * the lifetime of the corresponding instances of type Command.
   */
  class Optref final {
  public:
    /// @returns `true` if the instance is valid (references an option).
    bool is_valid() const noexcept
    {
      return is_valid_;
    }

    /**
     * @returns `is_valid()`.
     *
     * @par Requires
     * `!value()`.
     */
    bool is_valid_throw_if_value() const
    {
      const auto valid = is_valid();
      if (valid && value_)
        throw_requirement("requires no value");

      return valid;
    }

    /**
     * @returns `is_valid()`.
     *
     * @par Requires
     * `value()`.
     */
    bool is_valid_throw_if_no_value() const
    {
      const auto valid = is_valid();
      if (valid && !value_)
        throw_requirement("requires a value");

      return valid;
    }

    /// @returns `is_valid()`.
    explicit operator bool() const noexcept
    {
      return is_valid();
    }

    /// @returns The corresponding Command instance.
    const Command& command() const noexcept
    {
      return command_;
    }

    /// @returns The name of this option.
    const std::string& name() const
    {
      return name_;
    }

    /// @returns The value of this option.
    const std::optional<std::string>& value_of_optional() const
    {
      return value_;
    }

    /**
     * @returns The value of this mandatory option.
     *
     * @par Requires
     * `is_valid()`.
     */
    const std::optional<std::string>& value_of_mandatory() const
    {
      if (!is_valid())
        throw_requirement("is mandatory");

      return value_;
    }

    /**
     * @returns The not null value of this mandatory option.
     *
     * @par Requires
     * `value_of_mandatory()`.
     */
    const std::string& value_of_mandatory_not_null() const
    {
      const auto& val = value_of_mandatory();
      if (!val)
        throw_requirement("requires a value");

      return *val;
    }

    /**
     * @returns The not empty value of this mandatory option.
     *
     * @par Requires
     * `!value_of_mandatory_not_null().empty()`.
     */
    const std::string& value_of_mandatory_not_empty() const
    {
      const auto& val = value_of_mandatory_not_null();
      if (val.empty())
        throw_requirement("requires a non empty value");

      return val;
    }

  private:
    friend Command;

    bool is_valid_{};
    const Command& command_;
    std::string name_;
    std::optional<std::string> value_;

    /// The constructor. (Constructs invalid instance.)
    Optref(const Command& command, std::string name) noexcept
      : command_{command}
      , name_{std::move(name)}
    {
      DMITIGR_ASSERT(!is_valid());
    }

    /// The constructor.
    explicit Optref(const Command& command,
      std::string name, std::optional<std::string> value) noexcept
      : is_valid_{true}
      , command_{command}
      , name_{std::move(name)}
      , value_{std::move(value)}
    {
      DMITIGR_ASSERT(is_valid());
    }

    /// @throws `Exception`.
    [[noreturn]] void throw_requirement(const std::string_view requirement) const
    {
      DMITIGR_ASSERT(!requirement.empty());
      throw Exception{std::string{"option --"}
        .append(name_).append(" ").append(requirement)};
    }
  };

  /// The default constructor. (Constructs invalid instance.)
  Command() = default;

  /**
   * @brief The constructor.
   *
   * @par Requires
   * `!path.empty()`.
   */
  explicit Command(std::string name,
    Option_map options = {}, Parameter_vector parameters = {})
    : name_{std::move(name)}
    , options_{std::move(options)}
    , parameters_{std::move(parameters)}
  {
    if (name_.empty())
      throw Exception{"empty command name"};
    DMITIGR_ASSERT(is_valid());
  }

  /// @returns `false` if this instance is default-constructed.
  bool is_valid() const noexcept
  {
    return !name_.empty();
  }

  /// @returns `is_valid()`.
  explicit operator bool() const noexcept
  {
    return is_valid();
  }

  /// @returns The command name (or program path).
  const std::string& name() const noexcept
  {
    return name_;
  }

  /// @returns The map of options.
  const Option_map& options() const noexcept
  {
    return options_;
  }

  /// @returns The vector of parameters.
  const Parameter_vector& parameters() const noexcept
  {
    return parameters_;
  }

  /// @returns The option reference, or invalid instance if no option `name`.
  Optref option(const std::string& name) const noexcept
  {
    const auto i = options_.find(name);
    return i != cend(options_) ? Optref{*this, i->first, i->second} :
      Optref{*this, name};
  }

  /// @returns A value of type `std::tuple<Optref, ...>`.
  template<class ... Types>
  auto options(Types&& ... names) const noexcept
  {
    return std::make_tuple(option(std::forward<Types>(names))...);
  }

  /**
   * @returns options(names).
   *
   * @throw Exception if there is an option which doesn't present in `names`.
   */
  template<class ... Types>
  auto options_strict(Types&& ... names) const
  {
    const std::vector<std::string_view> opts{std::forward<Types>(names)...};
    for (const auto& kv : options_)
      if (find(cbegin(opts), cend(opts), kv.first) == cend(opts))
        throw Exception{std::string{"unexpected option --"}.append(kv.first)};
    return options(std::forward<Types>(names)...);
  }

  /// @returns `option(option_name)`.
  Optref operator[](const std::string& option_name) const noexcept
  {
    return option(option_name);
  }

  /**
   * @returns `parameters()[parameter_index]`.
   *
   * @par Requires
   * `(parameter_index < parameters().size())`.
   */
  const std::string& operator[](const std::size_t parameter_index) const
  {
    if (!(parameter_index < parameters_.size()))
      throw Exception{"invalid command parameter index"};
    return parameters_[parameter_index];
  }

private:
  std::string name_;
  Option_map options_;
  Parameter_vector parameters_;
};

/**
 * @returns The vector of parsed commands.
 *
 * @param argc The number of arguments in `argv`.
 * @param argv The arguments.
 *
 * @details Assumed syntax as follows:
 *   - one command mode: command [--option[=[value]]] [--] [parameter ...]
 *   - multicommand mode: command [--option[=[value]]] [-- [parameter ...]] ...
 *
 * Each option may have a value specified after the "=" character. The sequence
 * of two dashes ("--") indicates "end of options" (or "end of commands" in
 * multicommand mode), so the remaining arguments should be treated as parameters.
 *
 * @remarks Short options notation (e.g. `-o` or `-o 1`) doesn't supported
 * currently and always treated as parameters.
 *
 * @par Requires
 * `(argc > 0 && argv && argv[0] && std::strlen(argv[0]) > 0)`.
 */
inline std::vector<Command> parsed_commands(const int argc, const char* const* argv,
  const bool is_one_command_mode = {})
{
  if (!(argc > 0))
    throw Exception{"invalid count of arguments (argc)"};
  else if (!argv)
    throw Exception{"invalid vector of arguments (argv)"};

  static const auto is_opt = [](const std::string_view arg) noexcept
  {
    return arg.find("--") == 0;
  };

  static const auto opt = [](const std::string_view arg)
    -> std::optional<std::pair<std::string, std::optional<std::string>>>
    {
      if (is_opt(arg)) {
        if (arg.size() == 2) {
          // Explicit end-of-commands.
          return std::make_pair(std::string{}, std::nullopt);
        } else if (const auto pos = arg.find('=', 2); pos != std::string::npos) {
          // Option with value.
          auto name = arg.substr(2, pos - 2);
          auto value = arg.substr(pos + 1);
          return std::pair<std::string, std::string>(std::move(name),
            std::move(value));
        } else
          // Option without value.
          return std::make_pair(std::string{arg.substr(2)}, std::nullopt);
      } else
        // Not an option.
        return std::nullopt;
    };

  int argi{};
  std::vector<Command> result;
  while (argi < argc) {
    if (!argv[argi])
      throw Exception{"invalid vector of arguments (argv)"};

    std::string name{argv[argi]};
    if (name.empty())
      throw Exception{std::string{"empty command name at argv["}
        .append(std::to_string(argi)).append("]")};

    // Add another command.
    Command::Option_map options;
    Command::Parameter_vector parameters;

    // Increment argument index.
    ++argi;

    // Collect options.
    bool is_end_of_options_detected{};
    for (; argi < argc; ++argi) {
      if (auto o = opt(argv[argi])) {
        if (o->first.empty()) {
          is_end_of_options_detected = true;
          ++argi;
          break;
        } else
          options[std::move(o->first)] = std::move(o->second);
      } else
        // A command (or an parameter) detected.
        break;
    }

    // Collect parameters if the current command is the last one.
    if (is_end_of_options_detected || is_one_command_mode) {
      for (; argi < argc; ++argi)
        parameters.emplace_back(argv[argi]);
    }

    // Collect result.
    result.emplace_back(std::move(name), std::move(options), std::move(parameters));
  }
  DMITIGR_ASSERT(!result.empty());
  return result;
}

/// @overload
inline Command parsed_command(const int argc, const char* const* argv)
{
  return parsed_commands(argc, argv, true)[0];
}

namespace detail {
template<typename F, class C>
inline std::string command_id(const F& appender,
  const C& commands, const std::size_t offset, const std::string_view delim)
{
  std::string result;
  const auto sz = commands.size();
  if (!(offset < sz))
    throw Exception{"cannot generate command ID: offset is out of range"};
  const auto e = commands.cend();
  for (auto i = commands.cbegin() + offset; i != e; ++i)
    appender(result, *i, delim);
  result.pop_back();
  return result;
}
} // namespace detail

/// @returns The command identifier.
inline std::string command_id(const std::vector<Command>& commands,
  const std::size_t offset = 1, const std::string_view delim = ".")
{
  return detail::command_id([](std::string& result,
      const Command& command, const std::string_view delim)
  {
    result.append(command.name()).append(delim);
  }, commands, offset, delim);
}

/// @overload
inline std::string command_id(const std::vector<std::string>& parameters,
  const std::size_t offset = 0, const std::string_view delim = ".")
{
  return detail::command_id([](std::string& result,
      const std::string_view param, const std::string_view delim)
  {
    result.append(param).append(delim);
  }, parameters, offset, delim);
}


} // namespace dmitigr::prg

#endif // DMITIGR_PRG_COMMAND_HPP
