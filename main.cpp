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
        auto transition_builder = [direction](auto symbol)
        {
            return [direction, symbol](auto idx) -> turing_machine::transition_entry
            {
                return {{std::to_string(idx), symbol}, {{std::to_string(idx+1), symbol}, direction}};
            };
        };

        turing_machine tm {};
        tm.set_initial_state("0"sv);

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

    auto row_check(std::string_view name)
        -> turing_machine
    {
        turing_machine tm {
            {{"qStart", '_'}, {{"qAccept", '0'}, dir::right}}
        };

        // tm.set_title("Row"s + std::to_string(row));
        tm.set_title(name);
        return tm;
    }

    auto column_check(std::string_view name)
        -> turing_machine
    {
        return {};
    }
}


int main(int argc, char* argv[]) {
    if (argc != 2)
        terminate_message("Usage: ./tms <input>");

    auto first_mover{component::move_right(3, "Mover")};
    auto second_mover{component::move_left(5, "AnotherMover")};

    auto concat{turing_machine::multiconcat(
        turing_machine::list{first_mover, second_mover},
        component::alphabet, "UltraMover"
    )};

    run_input(concat, argv[1]);
}
