#include "turing.hpp"

#include <ranges>
#include <istream>

auto turing_machine::add_transition(tape_state state, tape_reaction reaction) -> void
{
    transitions[state] = reaction;
}

auto turing_machine::redirect_state(std::string_view state_from, std::string_view state_to, const std::set<char>& alphabet)
    -> void
{
    for (auto symbol : alphabet)
        add_transition(
            {std::string{state_from}, symbol},
            {{std::string{state_to}, symbol}, direction::hold}
        );
}

auto turing_machine::set_initial_state(std::string_view name) -> void
{
    initial = name;
}

auto turing_machine::set_accept_state(std::string_view name) -> void
{
    accept = name;
}

auto turing_machine::set_title(std::string_view title) -> void
{
    this->title = title;
}


auto turing_machine::load_input(std::string_view input) -> void
{
    current_state = initial;
    head_index = 0;
    tape_left = {};

    if (input.empty())
        tape_right = {blank_symbol};
    else
        tape_right = input | std::ranges::to<std::vector>();
}

static const std::unordered_map<std::string_view, turing_machine::direction> specifier_to_direction {
    {"<", turing_machine::direction::left},
    {">", turing_machine::direction::right},
    {"-", turing_machine::direction::hold}
};

auto direction_to_specifier = specifier_to_direction
    | std::views::transform([](const auto& pair){ return std::pair{pair.second, pair.first}; })
    | std::ranges::to<std::unordered_map<turing_machine::direction, std::string_view>>();

static auto split_line(std::string_view str, char s) {
    auto str_size = [](const auto& str) { return static_cast<std::size_t>(std::ranges::distance(str)); };
    auto subrange_to_str = [&](const auto& str) { return std::string_view{str.begin(), str_size(str)}; };

    return str
        | std::ranges::views::split(s)
        | std::ranges::views::transform(subrange_to_str)
        | std::ranges::to<std::vector<std::string_view>>();
}

bool is_space(char p) noexcept
{
    auto ne = [p](auto q) { return p != q; };
    return !!(" \t\n\v\r\f" | std::views::drop_while(ne));
};

static auto trim(std::string_view const str) -> std::string
{
    return str
        | std::views::drop_while(is_space)
        | std::views::reverse
        | std::views::drop_while(is_space)
        | std::views::reverse
        | std::ranges::to<std::string>();
}

std::istream& operator>>(std::istream& in, turing_machine& tm)
{
    std::string line{};

    std::getline(in, line);
    tm.set_initial_state(trim(split_line(line, ':')[1]));

    std::getline(in, line);
    tm.set_accept_state(trim(split_line(line, ':')[1]));
    
    while (std::getline(in, line)) {
        if (line.size() == 0)
            continue;
        
        if (line.starts_with("//"))
            continue;

        auto values_from{split_line(line, ',')};

        try {
            auto state_from{values_from.at(0)};
            auto symbol_from{values_from.at(1)[0]};   

            turing_machine::tape_state tape_state_from{state_from, symbol_from};

            std::getline(in, line);
            auto values_to{split_line(line, ',')};

            auto state_to{values_to.at(0)};
            auto symbol_to{values_to.at(1)[0]};
            auto direction{specifier_to_direction.at(values_to.at(2))};

            turing_machine::tape_state tape_state_to{state_to, symbol_to};
            turing_machine::tape_reaction reaction{tape_state_to, direction};

            tm.add_transition(tape_state_from, reaction);
        } catch (std::out_of_range const&) {
            throw std::logic_error("Invalid format for Turing machine description");
        }
    }

    return in;
}

std::ostream& operator<<(std::ostream& out, const turing_machine& tm)
{
    out << "init: " << tm.initial << std::endl
        << "accept: " << tm.accept << std::endl << std::endl;

    for (auto const& [key, val] : tm)
        out << std::format("{},{}", key.first, key.second) << std::endl
            << std::format("{},{},{}", val.first.first, val.first.second,
                direction_to_specifier.at(val.second))
            << std::endl << std::endl;

    return out;
}

auto turing_machine::prefix(std::string str) const
    -> turing_machine
{
    return transform_states([&](std::string_view s) {
        return std::format("[{}]{}", str, s);
    });
}

auto turing_machine::prefixed() const -> turing_machine
{
    return prefix(title);
}

auto turing_machine::transform_states(std::function<std::string(std::string_view)> callback) const
    -> turing_machine
{
    turing_machine::transition_table result_transitions{};

    for (const auto& [state, reaction] : transitions) {
        result_transitions[{callback(state.first), state.second}]
            = {{callback(reaction.first.first), reaction.first.second}, reaction.second};
    }

    turing_machine result{result_transitions};
    result.set_initial_state(callback(initial));
    result.set_accept_state(callback(accept));
    result.set_title(title);

    return result;
}

auto turing_machine::step() -> status {
    const std::unordered_map<direction, std::ptrdiff_t> index_diff {
        {direction::left, -1},
        {direction::right, 1},
        {direction::hold,  0}
    };

    auto& current_symbol{
        head_index >= 0 ? tape_right.at(head_index)
            : tape_left[-head_index - 1]
    };

    tape_state state_from{current_state, current_symbol};
    if (!transitions.contains(state_from))
        return status::reject;

    auto reaction = transitions.at(state_from);

    current_state = reaction.first.first;
    head_index += index_diff.at(reaction.second);

    // Change symbol at head position on tape (current_symbol is reference)
    current_symbol = reaction.first.second;

    if (head_index == static_cast<std::ptrdiff_t>(tape_right.size()))
        tape_right.push_back(blank_symbol);
    
    if (-head_index - 1 == static_cast<std::ptrdiff_t>(tape_left.size()))
        tape_left.push_back(blank_symbol);
    
    return current_state == halt_state ? status::halt
        : current_state == accept ? status::accept
        : status::running;
}

auto turing_machine::tape() const -> std::string {
    return std::string{tape_left.rbegin(), tape_left.rend()}
         + std::string{tape_right.begin(), tape_right.end()};
}

auto turing_machine::head() const -> std::string {
    auto left_size = tape_left.size();
    auto right_size = tape_right.size();

    return std::string(left_size + head_index, '_') + 'v'
        + std::string(right_size - head_index - 1, '_')
        + " (" + current_state + ')';
}

auto turing_machine::status_message(status exec) -> std::string_view {
    return std::unordered_map<status, std::string_view> {
        {status::accept, "Machine accepted."},
        {status::reject, "Machine rejected."},
        {status::halt, "Machine halted."}
    }.at(exec);
}

auto turing_machine::tape_state_hash::operator()(const tape_state& state) const -> std::size_t {
    auto h1 = std::hash<std::string>()(state.first);
    auto h2 = std::hash<char>()(state.second);
    return h1 ^ (h2 << 1);
}
