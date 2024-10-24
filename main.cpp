#include <__ranges/repeat_view.h>
#include <algorithm>
#include <concepts>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
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

    auto first_chars = [](const auto& seq, int n)
    {
        return seq | std::views::take(n);
    };

    auto last_symbol = [](auto seq)
    {
        return *(seq | std::views::reverse).begin();
    };

    auto to_string = [](auto seq)
    {
        return seq | std::ranges::to<std::string>();
    };

    constexpr auto permutations_sequence()
        -> std::vector<std::vector<char>>
    {
        std::vector<char> set{'1', '2', '3', '4'};
        std::vector<std::vector<char>> sequences{};
        
        do sequences.push_back(set);
        while (std::ranges::next_permutation(set).found);

        return sequences;
    }

    template<std::ranges::forward_range R>
    requires std::convertible_to<std::ranges::range_reference_t<R>, char>
    auto row_expect(R sequence, std::string_view name)
        -> turing_machine
    {
        turing_machine tm{};
        tm.set_initial_state("start");

        auto seq_len{std::ranges::distance(sequence)};
        auto first{last_symbol(first_chars(sequence, 1))};
        auto last{last_symbol(sequence)};

        tm.add_transitions(turing_machine::transition_table{
            // Transition from start to subsequence of 1
            {
                {tm.initial_state(), first},
                {{to_string(first_chars(sequence, 1)), first}, dir::right}
            },

            // Transition from complete subsequence to accept
            {
                {to_string(first_chars(sequence, seq_len-1)), last},
                {{tm.accept_state(), last}, dir::hold}
            }
        });

        for (const auto n : std::views::iota(0)
            | std::views::take(seq_len)
            | std::views::drop(1) | std::views::reverse // Drop first (subsequence of 0)
            | std::views::drop(1) | std::views::reverse // Drop last (subsequence = sequence)
        ) {
            auto next_subseq{first_chars(sequence, n+1)};
            auto subseq{next_subseq | std::views::reverse
                | std::views::drop(1) | std::views::reverse};

            // Transition from n-subsequence to (n+1)-subsequence
            tm.add_transition(
                {to_string(subseq), last_symbol(next_subseq)},
                {{to_string(next_subseq), last_symbol(next_subseq)}, dir::right}
            );
        }

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

    auto check_row(std::string_view name)
        -> turing_machine
    {
        auto perm{permutations_sequence()};
        return turing_machine::multiunion(perm | std::views::transform([&](const auto& seq) {
            return row_expect(seq, name);
        }), name);
    }

    auto check_rows(std::string_view name)
        -> turing_machine
    {
        return turing_machine::multiconcat(
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
            alphabet, name
        );
    }

    constexpr auto tower_sequence(bool colon)
        -> std::vector<std::vector<char>>
    {
        std::vector<char> set{'1', '2', '3', '4'};
        std::vector<std::vector<char>> sequences{};
        
        for (const auto tower : std::views::iota(1) | std::views::take(4)) {
            do {
                char max_height{0};
                int towers_visible{0};
                for (const auto height : set)
                    if (height > max_height)
                        max_height = height, towers_visible++;
                
                char tower_symbol{static_cast<char>('0' + tower)};

                if (towers_visible == tower) {
                    if (colon)
                        sequences.push_back({tower_symbol, ':', set[0], set[1], set[2]});
                    else
                        sequences.push_back({tower_symbol, set[0], set[1], set[2]});
                }
            } while (std::ranges::next_permutation(set).found);
        }

        return sequences;
    }

    auto tower_left(std::string_view name)
        -> turing_machine
    {
        auto tower_seq{tower_sequence(true)};
        return turing_machine::multiunion(tower_seq
            | std::views::transform([&](const auto& seq) {
                return row_expect(seq, name);
            }
        ), name);
    }

    auto tower_right(std::string_view name)
        -> turing_machine
    {
        auto tower_seq{tower_sequence(true)};
        return turing_machine::multiunion(tower_seq
            // Reverse every sequence
            | std::views::transform([](const auto& seq) {
                return seq | std::views::reverse | std::ranges::to<std::vector<char>>();
            })
            // Construct corresponding expecter
            | std::views::transform([&](const auto& seq) {
                return row_expect(seq, name);
            }
        ), name);
    }

    auto towers_rows(std::string_view name)
        -> turing_machine
    {
        return turing_machine::multiconcat(
            turing_machine::list{
                find_right(':', "move_to_tower1:"),

                repeat(turing_machine::multiconcat(
                    turing_machine::list{
                        move_left(1, "pass:"),
                        tower_left("tower_left"),
                        move_left(1, "move_to_right_tower"),
                        tower_right("tower_right"),
                        move_right(3, "move_to_next"),
                    },
                    alphabet, "loop_body"
                ), repeater::do_while, ':', "tower_loop"),

                find_left('_', "move_back"),
                consume_right('_', "move_to_start")
            },
            alphabet, name
        );
    }

    template<std::ranges::forward_range R, std::ranges::forward_range Q>
    requires std::convertible_to<std::ranges::range_reference_t<R>, std::vector<char>>
        && std::convertible_to<std::ranges::range_reference_t<Q>, int>
    auto col_expect(R sequences, Q distances, std::string_view name)
        -> turing_machine
    {
        auto build_carrier = [](std::string expect, int distance)
        {
            return move_right(distance, "shift").prefix(expect);
        };

        turing_machine tm{};
        tm.set_initial_state("start");

        std::unordered_map<std::string, turing_machine> carriers;
        
        for (const auto& seq : sequences) {
            auto expect_1{to_string(first_chars(seq, 1))};
            auto expect_2{to_string(first_chars(seq, 2))};
            auto expect_3{to_string(first_chars(seq, 3))};

            auto dist_iter{distances.begin()};
            carriers[expect_1] = build_carrier(expect_1, *dist_iter++);
            carriers[expect_2] = build_carrier(expect_2, *dist_iter++);
            carriers[expect_3] = build_carrier(expect_3, *dist_iter++);

            tm.add_transitions(turing_machine::transition_table{
                {
                    {tm.initial_state(), seq[0]},
                    {{carriers.at(expect_1).initial_state(), seq[0]}, dir::hold}
                },
                {
                    {carriers.at(expect_1).accept_state(), seq[1]},
                    {{carriers.at(expect_2).initial_state(), seq[1]}, dir::hold}
                },
                {
                    {carriers.at(expect_2).accept_state(), seq[2]},
                    {{carriers.at(expect_3).initial_state(), seq[2]}, dir::hold}
                },
                {
                    {carriers.at(expect_3).accept_state(), seq[3]},
                    {{tm.accept_state(), seq[3]}, dir::hold}
                }
            });
        }

        auto carriers_tm{turing_machine::multiunion(carriers | std::views::values, "carriers")};
        return turing_machine::multiunion(turing_machine::list{tm, carriers_tm}, name);
    }

    auto check_col(std::string_view name)
        -> turing_machine
    {
        return col_expect(permutations_sequence(), std::views::repeat(9), name);
    }

    auto check_cols(std::string_view name)
        -> turing_machine
    {
        return turing_machine::multiconcat(
            turing_machine::list{
                find_right(':', "move_to_col1:"),
                consume_right(':', "pass:"),

                repeat(turing_machine::multiconcat(
                    turing_machine::list{
                        check_col("tower_up"),
                        move_left(26, "move_to_next")        
                    },
                    alphabet, "loop_body"
                ), repeater::do_until, ':', "col_loop"),

                find_left('_', "move_back"),
                consume_right('_', "move_to_start")
            },
            alphabet, name
        );
    }

    auto tower_up(std::string_view name)
        -> turing_machine
    {
        return col_expect(tower_sequence(false), std::vector<int>{7, 9, 9}, name);
    }

    auto tower_down(std::string_view name)
        -> turing_machine
    {
        return col_expect(tower_sequence(false) | std::views::transform([](const auto& seq) {
            return seq | std::views::reverse | std::ranges::to<std::vector<char>>();
        }), std::vector<int>{9, 9, 7}, name);
    }

    auto towers_cols(std::string_view name)
        -> turing_machine
    {
        return turing_machine::multiconcat(
            turing_machine::list{
                repeat(turing_machine::multiconcat(
                    turing_machine::list{
                        tower_up("tower_up"),
                        move_left(9, "move_to_down"),
                        tower_down("tower_down"),
                        move_left(40, "move_to_next")
                    },
                    alphabet, "loop_body"
                ), repeater::do_until, '#', "tower_loop"),

                find_left('_', "move_back"),
                consume_right('_', "move_to_start")
            },
            alphabet, name
        );
    }
}

int main(int argc, char* argv[]) {
    if (argc > 2)
        terminate_message("Usage: ./tms [input]");

    auto concat{turing_machine::multiconcat(
        turing_machine::list{
            // component::check_rows("check_rows"),
            component::check_cols("check_cols"),
            // component::towers_rows("towers_rows"),
            // component::towers_cols("towers_cols")
        },
        component::alphabet, "solver"
    )};

    concat.redirect_state(concat.accept_state(), "Y", component::alphabet);

    std::cout << concat;

    if (argc == 2)
        run_input(concat, argv[1]);
}
