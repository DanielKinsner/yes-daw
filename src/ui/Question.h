// YES DAW - a question the shell asks (ADR-0077).
//
// A question is answered at once - a test's function that returns the answer - or later: the native shell's chooser or
// alert is handed a reply and calls it when the user answers. Either way the asker's continuation receives the answer,
// or nothing when the question was refused (another question was open, there was no one to ask, the reply was dropped
// unanswered). No nested modal loop waits for it.

#pragma once

#include <juce_core/juce_core.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace yesdaw::ui {

class MainComponent;

// The reply a question that answers later is handed (ADR-0077 rule 2). Copies share one state: the first call answers,
// a later call is ignored (and asserts); when the last copy goes without a call, the asker hears "refused".
template <typename Answer>
class QuestionReply
{
public:
    using Continuation = std::function<void (std::optional<Answer>)>;

    QuestionReply() = default;
    explicit QuestionReply (Continuation then) : state (std::make_shared<State> (std::move (then))) {}

    void operator() (Answer answer) const
    {
        if (state == nullptr || state->done)
        {
            jassertfalse;   // a reply answers once
            return;
        }
        state->finish (std::optional<Answer> (std::move (answer)));
    }

private:
    struct State
    {
        explicit State (Continuation c) : then (std::move (c)) {}
        State (const State&) = delete;
        State& operator= (const State&) = delete;
        ~State()
        {
            if (! done)
                finish (std::nullopt);   // dropped unanswered: refused
        }
        void finish (std::optional<Answer> answer)
        {
            done = true;
            if (Continuation c = std::exchange (then, nullptr))
                c (std::move (answer));
        }
        Continuation then;
        bool done = false;
    };
    std::shared_ptr<State> state;
};

// A question with an answer of type `Answer`, asked with `Args`: built from a function that returns the answer (answered
// at once) or from one that takes the arguments and a reply (answered later). Only MainComponent asks, through its one
// ask helper (ADR-0077 rules 1-2).
template <typename Answer, typename... Args>
class Question
{
public:
    using Reply = QuestionReply<Answer>;

    Question() = default;
    Question (std::nullptr_t) noexcept {}

    template <typename F>
        requires (std::is_invocable_v<F&, const Args&..., Reply>)
    Question (F later) : ask (std::move (later))
    {
    }

    template <typename F>
        requires (! std::is_invocable_v<F&, const Args&..., Reply> && std::is_invocable_r_v<Answer, F&, const Args&...>)
    Question (F atOnce)
        : ask ([f = std::move (atOnce)] (const Args&... args, Reply reply) mutable { reply (f (args...)); })
    {
    }

    explicit operator bool() const noexcept { return static_cast<bool> (ask); }

private:
    friend class MainComponent;
    void operator() (const Args&... args, Reply reply) const { ask (args..., std::move (reply)); }

    std::function<void (const Args&..., Reply)> ask;
};

} // namespace yesdaw::ui
