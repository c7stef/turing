#include <__ranges/repeat_view.h>
#include <algorithm>
#include <concepts>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <ranges>
#include <string>
#include <string_view>
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
        auto build_transition = [direction](const auto symbol)
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
        tm.set_initial_state(std::to_string(0));
        tm.set_accept_state(std::to_string(amount));

        for (const auto symbol : alphabet) {
            tm.add_transitions(
                std::views::iota(0)
                | std::views::take(amount)
                | std::views::transform(build_transition(symbol))
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
        auto repeater{turing_machine::concat(
            turing_machine::list{tm}, name)
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

    auto consume(char symbol, dir direction, std::string_view name)
        -> turing_machine
    {
        turing_machine tm{};
        tm.set_initial_state("consume");
        tm.add_transition({tm.initial_state(), symbol}, {{tm.accept_state(), symbol}, direction});
        tm.set_title(name);
        return tm;
    }

    template<std::ranges::forward_range R, std::ranges::forward_range Q>
    requires std::convertible_to<std::ranges::range_reference_t<R>, char>
        && std::convertible_to<std::ranges::range_reference_t<Q>, int>
    auto expect(R sequence, dir direction, Q distances, std::string_view name)
        -> turing_machine
    {
        auto seq_len{std::ranges::distance(sequence)};

        auto drop_last = [](const std::ranges::forward_range auto range)
        {
            return range | std::views::reverse | std::views::drop(1) | std::views::reverse;
        };

        auto carrier_name = [&](const std::ranges::forward_range auto expect)
        {
            return expect.size() == 1 ? "start"s : to_string(drop_last(expect));
        };

        auto nth_expect_distance = [&](const auto n)
        {
            return *std::ranges::next(distances.begin(), n-2);
        };

        auto build_carrier = [&](std::string expect)
        {
            auto len{std::ranges::distance(expect)};

            turing_machine::list carrier_parts{
                consume(last_symbol(expect), direction, "check"),
            };

            auto distance{nth_expect_distance(len)};
            if (distance > 1)
                carrier_parts.push_front(_move(distance-1, "shift", direction));

            return turing_machine::concat(carrier_parts, carrier_name(expect));
        };


        auto carrier_subseqs_lengths{
            std::views::iota(2)
            | std::views::take(seq_len-1)
        };

        // Map n-subsequences to carriers
        auto carriers{
            carrier_subseqs_lengths
            | std::views::transform([&](const auto len) {
                return build_carrier(to_string(first_chars(sequence, len)));
            })
            | std::ranges::to<turing_machine::list>()
        };

        // Map start (0-subsequence) to 1-subsequence
        auto first_subseq{first_chars(sequence, 1)};
        carriers.push_front(consume(last_symbol(first_subseq), direction, carrier_name(first_subseq)));

        auto expecter{turing_machine::concat(carriers, name)};
        expecter.redirect_state(expecter.accept_state(), "Y", alphabet);
        expecter.set_accept_state("Y");
        return expecter;
    }

    constexpr auto permutations_sequence()
        -> std::vector<std::vector<char>>
    {
        std::vector<char> set{'1', '2', '3', '4'};
        std::vector<std::vector<char>> sequences{};
        
        do sequences.push_back(set);
        while (std::ranges::next_permutation(set).found);

        return sequences;
    }

    auto check_row(std::string_view name)
        -> turing_machine
    {
        auto perm{permutations_sequence()};
        return turing_machine::union_all(perm | std::views::transform([&](const auto& seq) {
            return expect(seq, dir::right, std::views::repeat(1), name);
        }), name);
    }

    auto check_rows(std::string_view name)
        -> turing_machine
    {
        return turing_machine::concat(
            turing_machine::list{
                find_right(':', "move_to_row1:"),

                repeat(turing_machine::concat(
                    turing_machine::list{
                        consume(':', dir::right, "pass:"),
                        check_row("check_row"),
                        move_right(4, "move_to_next")        
                    }, "loop_body"
                ), repeater::do_while, ':', "row_loop"),

                find_left('_', "move_back"),
                consume('_', dir::right, "move_to_start")
            }, name
        );
    }

    auto check_col(std::string_view name)
        -> turing_machine
    {
        auto perm{permutations_sequence()};
        return turing_machine::union_all(perm | std::views::transform([&](const auto& seq) {
            return expect(seq, dir::right, std::views::repeat(9), name);
        }), name);
    }

    auto check_cols(std::string_view name)
        -> turing_machine
    {
        return turing_machine::concat(
            turing_machine::list{
                find_right(':', "move_to_col1:"),
                consume(':', dir::right, "pass:"),

                repeat(turing_machine::concat(
                    turing_machine::list{
                        check_col("check_col"),
                        move_left(27, "move_to_next")        
                    }, "loop_body"
                ), repeater::do_until, ':', "col_loop"),

                find_left('_', "move_back"),
                consume('_', dir::right, "move_to_start")
            }, name
        );
    }

    constexpr auto tower_sequence()
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
                    sequences.push_back({tower_symbol, set[0], set[1], set[2]});
                }
            } while (std::ranges::next_permutation(set).found);
        }

        return sequences;
    }

    enum class row_tower {
        left,
        right
    };

    auto tower_row(row_tower tower, std::string_view name)
        -> turing_machine
    {
        auto tower_seq{tower_sequence()};
        auto expect_dir{tower == row_tower::left ? dir::right : dir::left};

        return turing_machine::union_all(tower_seq
            | std::views::transform([&](const auto& seq) {
                return expect(seq, expect_dir, std::vector{2, 1, 1}, name);
            }
        ), name);
    }

    auto towers_rows(std::string_view name)
        -> turing_machine
    {
        return turing_machine::concat(
            turing_machine::list{
                find_right(':', "move_to_tower1:"),

                repeat(turing_machine::concat(
                    turing_machine::list{
                        move_left(1, "pass:"),
                        tower_row(row_tower::left, "tower_left"),
                        move_right(2, "move_to_right_tower"),
                        tower_row(row_tower::right, "tower_right"),
                        move_right(8, "move_to_next"),
                    }, "loop_body"
                ), repeater::do_while, ':', "tower_loop"),

                find_left('_', "move_back"),
                consume('_', dir::right, "move_to_start")
            }, name
        );
    }

    enum class col_tower {
        down,
        up
    };

    auto tower_col(col_tower tower, std::string_view name)
        -> turing_machine
    {
        auto tower_seq{tower_sequence()};
        auto expect_dir{tower == col_tower::up ? dir::right : dir::left};

        return turing_machine::union_all(tower_seq
            | std::views::transform([&](const auto& seq) {
                return expect(seq, expect_dir, std::vector{7, 9, 9}, name);
            }
        ), name);
    }

    auto towers_cols(std::string_view name)
        -> turing_machine
    {
        return turing_machine::concat(
            turing_machine::list{
                repeat(turing_machine::concat(
                    turing_machine::list{
                        tower_col(col_tower::up, "tower_up"),
                        move_right(15, "move_to_down"),
                        tower_col(col_tower::down, "tower_down"),
                        move_left(14, "move_to_next")
                    }, "loop_body"
                ), repeater::do_until, '#', "tower_loop"),

                find_left('_', "move_back"),
                consume('_', dir::right, "move_to_start")
            }, name
        );
    }
}

int main(int argc, char* argv[]) {
    if (argc > 2)
        terminate_message("Usage: ./tms [input]");

    auto concat{turing_machine::concat(
        turing_machine::list{
            component::check_rows("check_rows"),
            component::check_cols("check_cols"),
            component::towers_rows("towers_rows"),
            component::towers_cols("towers_cols")
        }, "solver"
    )};

    concat.redirect_state(concat.accept_state(), "Y", component::alphabet);

    std::cout << concat;

    if (argc == 2)
        run_input(concat, argv[1]);
}
