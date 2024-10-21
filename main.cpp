#include <algorithm>
#include <initializer_list>
#include <iostream>
#include <ranges>
#include <string>
#include <string_view>
#include "turing.hpp"

using namespace std::literals;

[[noreturn]] void terminate_message(std::string_view message)
{
    std::cerr << message << std::endl;
    std::exit(EXIT_FAILURE);
}

auto ansi_blue{"\033[1;34m"sv};
auto ansi_reset{"\033[0m"sv};

void run_input(turing_machine& tm, std::string_view input)
{
    auto print_tm_state = [](const auto& tm)
    {
        std::cout << tm.head() << std::endl
            << ansi_blue << tm.tape() << ansi_reset << std::endl << std::endl;
    };

    tm.load_input(input);
    print_tm_state(tm);

    turing_machine::status status{};
    do {
        status = tm.step();
        print_tm_state(tm);
    } while (status == turing_machine::status::running);

    std::cout << turing_machine::status_message(status) << std::endl;
}

turing_machine read_tm(std::istream& in)
{
    turing_machine tm{};

    try {
        in >> tm;
    } catch (std::exception const& exception) {
        terminate_message(exception.what());
    }

    return tm;
}


namespace component {
    using dir = turing_machine::direction;
    const auto alphabet{"1234:#_"sv | std::ranges::to<std::set>()};

    auto _move(int amount, std::string_view name, dir direction)
        -> turing_machine
    {
        auto transition_builder = [direction](const auto symbol)
        {
            return [direction, symbol](const auto idx) -> turing_machine::transition_entry
            {
                return {
                     {std::to_string(idx), symbol},
                    {{std::to_string(idx+1), symbol}, direction}
                };
            };
        };

        turing_machine tm {};
        tm.set_initial_state("0");

        for (const auto symbol : alphabet) {
            tm.add_transitions(
                std::views::iota(0)
                | std::views::take(amount)
                | std::views::transform(transition_builder(symbol))
            );

            tm.add_transition(
                {std::to_string(amount), symbol},
                {{tm.accept_state(), symbol}, dir::hold}
            );
        }
        
        tm.set_title(name);
        return tm;
    }

    auto move_right(int amount, std::string_view name)
        -> turing_machine
    {
        return _move(amount, name, dir::right);
    }

    auto move_left(int amount, std::string_view name)
        -> turing_machine
    {
        return _move(amount, name, dir::left);
    }

    auto find(char needle, std::string_view name, dir direction)
        -> turing_machine
    {
        turing_machine tm {};
        tm.set_initial_state("search");

        for (const auto symbol : alphabet) {
            auto is_needle{symbol == needle};

            tm.add_transition(
                {"search", symbol},
                {{is_needle ? tm.accept_state() : "search", symbol},
                    is_needle ? dir::hold : direction}
            );
        }
        
        tm.set_title(name);
        return tm;
    }

    auto find_right(char needle, std::string_view name)
        -> turing_machine
    {
        return find(needle, name, dir::right);
    }

    auto find_left(char needle, std::string_view name)
        -> turing_machine
    {
        return find(needle, name, dir::left);
    }

    enum class repeater {
        do_until,
        do_while
    };

    auto repeat(const turing_machine& tm, repeater type, char symbol, std::string_view name)
        -> turing_machine
    {
        // Start with prefixed renamed version of tm
        auto repeater{turing_machine::multiconcat(
            turing_machine::list{tm},
            alphabet, name)
        };

        auto checker_state{"check"};
        auto break_state{"break"};

        // Redirect accept -> check
        repeater.redirect_state(repeater.accept_state(), checker_state, alphabet);

        // Redirect check -> initial [do_until] or break out [do_while]
        repeater.redirect_state(checker_state,
            type == repeater::do_until ? repeater.initial_state()
                : break_state,
            alphabet
        );

        // ...but (check, needle) -> break out [do_until] or continue [do_while]
        repeater.add_transition({checker_state, symbol}, {{
            type == repeater::do_until ? break_state
                : repeater.initial_state(),
            symbol
        }, dir::hold});
        repeater.set_accept_state(break_state);

        return repeater;
    }

    auto check_row(std::string_view name)
        -> turing_machine
    {
        auto state_builder = [](const auto& perm, const auto first_n)
            -> std::string
        {
            return perm | std::views::take(first_n) | std::ranges::to<std::string>();
        };

        turing_machine tm{};
        tm.set_initial_state("check");

        std::vector<char> expect{'1', '2', '3', '4'};

        for (const auto symbol : expect)
            tm.add_transition({tm.initial_state(), symbol}, {{std::string{symbol}, symbol}, dir::right});

        do {
            tm.add_transitions(turing_machine::transition_table{
                {{state_builder(expect, 1), expect[1]}, {{state_builder(expect, 2), expect[1]}, dir::right}},
                {{state_builder(expect, 2), expect[2]}, {{state_builder(expect, 3), expect[2]}, dir::right}},
                {{state_builder(expect, 3), expect[3]}, {{tm.accept_state(), expect[3]}, dir::hold}},
            });
        } while (std::next_permutation(expect.begin(), expect.end()));

        tm.set_title(name);
        return tm;
    }

    auto consume_right(char symbol, std::string_view name)
        -> turing_machine
    {
        turing_machine tm{};
        tm.set_initial_state(std::string{symbol});
        tm.add_transition({tm.initial_state(), symbol}, {{tm.accept_state(), symbol}, dir::right});
        tm.set_title(name);
        return tm;
    }

    auto check_rows(std::string_view name)
        -> turing_machine
    {
        turing_machine tm{turing_machine::multiconcat(
            turing_machine::list{
                find_right(':', "move_to_row1:"),

                repeat(turing_machine::multiconcat(
                    turing_machine::list{
                        consume_right(':', "pass:"),
                        check_row("check_row"),
                        move_right(5, "move_to_next")        
                    },
                    alphabet, "loop_body"
                ), repeater::do_while, ':', "row_loop"),

                find_left('_', "move_back"),
                consume_right('_', "move_to_start")
            },
            alphabet, "check_rows"
        )};

        tm.set_title(name);
        return tm;
    }

    auto check_cols(std::string_view name)
        -> turing_machine
    {
        return {};
    }
}


int main(int argc, char* argv[]) {
    if (argc > 2)
        terminate_message("Usage: ./tms [input]");

    auto rows{component::check_rows("check_rows")};

    auto concat{turing_machine::multiconcat(
        turing_machine::list{rows},
        component::alphabet, "solver"
    )};

    concat.redirect_state(concat.accept_state(), "Y", component::alphabet);

    std::cout << concat;

    if (argc == 2)
        run_input(concat, argv[1]);
}
