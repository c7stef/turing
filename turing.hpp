#ifndef TURING_H
#define TURING_H

#include <algorithm>
#include <functional>
#include <string_view>
#include <concepts>
#include <vector>
#include <unordered_map>
#include <initializer_list>
#include <utility>
#include <ranges>
#include <list>
#include <set>

class turing_machine {
public:
    using tape_state = std::pair<std::string, char>;

    // Stupid hash map needs a hash function for some reason...
    struct tape_state_hash {
        auto operator()(const tape_state& state) const -> std::size_t;
    };

    enum class direction {
        left,
        right,
        hold
    };

    using tape_reaction = std::pair<tape_state, direction>;
    using transition_entry = std::pair<tape_state, tape_reaction>;
    using transition_table = std::unordered_map<tape_state, tape_reaction, tape_state_hash>;

    // using ref = std::reference_wrapper<const turing_machine>;
    using list = std::list<turing_machine>;

    turing_machine()
    {
    }

    turing_machine(std::initializer_list<transition_entry> transitions)
        : transitions{transitions | std::ranges::to<transition_table>()}
    {
    }

    template<typename R>
    requires std::ranges::range<R>
        && std::convertible_to<std::ranges::range_value_t<R>, transition_entry>
    turing_machine(R transitions)
        : transitions{transitions | std::ranges::to<transition_table>()}
    {
    }

    enum class status {
        accept,
        reject,
        halt,
        running
    };

    auto add_transition(tape_state state, tape_reaction reaction) -> void;

    template<typename R>
    requires std::ranges::range<R>
        && std::convertible_to<std::ranges::range_value_t<R>, transition_entry>
    auto add_transitions(R transitions) -> void
    {
        this->transitions.merge(transitions | std::ranges::to<transition_table>());
    }

    auto begin() const -> transition_table::const_iterator { return transitions.begin(); }
    auto end() const -> transition_table::const_iterator { return transitions.end(); }

    auto redirect_state(std::string_view state_from, std::string_view state_to, const std::set<char>& alphabet)
        -> void;
    auto set_initial_state(std::string_view name) -> void;
    auto set_accept_state(std::string_view name) -> void;
    auto set_title(std::string_view title) -> void;

    auto load_input(std::string_view input) -> void;
    auto step() -> status;
    
    auto tape() const -> std::string;
    auto head() const -> std::string;
    
    static auto status_message(status exec) -> std::string_view;

    static auto concat(
        const turing_machine& first,
        const turing_machine& second,
        const std::set<char>& alphabet,
        std::string_view title
    )
        -> turing_machine;
    
    template<typename R>
    requires std::ranges::range<R>
        && std::convertible_to<std::ranges::range_value_t<R>, turing_machine>
    static auto multiconcat(
        R tms,
        const std::set<char>& alphabet,
        std::string_view title
    )
        -> turing_machine
    {
        // Get first Turing machine (prefixed)
        auto initial = (*tms.begin()).prefixed();
        
        auto result = std::ranges::fold_left(tms | std::views::drop(1), initial,
            [&](const turing_machine& first, const turing_machine& second)
        {
            auto result{first};
            auto prefixed_second = second.prefixed();

            result.redirect_state(result.accept, prefixed_second.initial, alphabet);
            result.add_transitions(prefixed_second.transitions);
            result.set_accept_state(prefixed_second.accept);

            return result;
        });

        result.set_title(title);
        return result;
    }

    template<typename R>
    requires std::ranges::range<R>
        && std::convertible_to<std::ranges::range_value_t<R>, turing_machine>
    static auto multiunion(
        R tms,
        std::string_view title
    )
        -> turing_machine
    {
        auto initial = *tms.begin();
        
        auto result = std::ranges::fold_left(tms | std::views::drop(1), initial,
            [&](const turing_machine& first, const turing_machine& second)
        {
            auto result{first};
            result.add_transitions(second.transitions);
            return result;
        });

        result.set_title(title);
        return result;
    }

    auto transform_states(std::function<std::string(std::string_view)> callback) const
        -> turing_machine;
    
    auto prefix(std::string str) const
        -> turing_machine;

    auto initial_state() const -> std::string { return initial; }
    auto accept_state() const -> std::string { return accept; }

private:
    transition_table transitions{};

    std::string initial{"qStart"};
    std::string halt_state{"H"};
    std::string accept{"Y"};
    std::string title{"MyMachine"};

    static constexpr char blank_symbol{'_'};

    std::vector<char> tape_right{};
    std::vector<char> tape_left{};
    std::ptrdiff_t head_index{0};
    std::string current_state{initial};

    auto prefixed() const -> turing_machine;

    friend std::ostream& operator<<(std::ostream& out, const turing_machine& tm);
};

std::istream& operator>>(std::istream& in, turing_machine& tm);
std::ostream& operator<<(std::ostream& out, const turing_machine& tm);

#endif