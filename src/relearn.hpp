#ifndef RELEARN_HPP
#define RELEARN_HPP
/**
 * Copyright 2015 Alexander Giokas
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * #http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <unordered_map>
#include <deque>
#include <functional>
#include <algorithm>
#include <memory>
#include <cassert>
#if USING_BOOST_SERIALIZATION
#include "serialize.tpl"
#endif
/** 
 
   ________  _______   ___       _______   ________  ________  ________      
  |\   __  \|\  ___ \ |\  \     |\  ___ \ |\   __  \|\   __  \|\   ___  \
  \ \  \|\  \ \   __/|\ \  \    \ \   __/|\ \  \|\  \ \  \|\  \ \  \\ \  \
   \ \   _  _\ \  \_|/_\ \  \    \ \  \_|/_\ \   __  \ \   _  _\ \  \\ \  \
    \ \  \\  \\ \  \_|\ \ \  \____\ \  \_|\ \ \  \ \  \ \  \\  \\ \  \\ \  \
     \ \__\\ _\\ \_______\ \_______\ \_______\ \__\ \__\ \__\\ _\\ \__\\ \__\
      \|__|\|__|\|_______|\|_______|\|_______|\|__|\|__|\|__|\|__|\|__| \|__|
 

 * @brief relearn C++ reinforcement learning library
 * @version 0.1.1
 * @date 07-July-2017
 * @author Alex Giokas <alexge233@hotmail.com>
 *
 * @note: do not confuse `state_class` with `state_trait`
 * and similarly, `action_class` with `action_trait`.
 * Whereas `state_class` refers to the template parameter being used
 * in class `policy` the `state_trait` is what characterises a `state_cass`,
 * or similarly an action. Essentially, the template parameter `state_class` 
 * used later in policy, refers to template class `state` wrapper but because this
 * is a template-based library, you must roll out your own `state` class(es) provided
 * that it:
 *	- is hashable
 *  - is comparable
 * Similary `action` takes the same approach.
 */
namespace relearn 
{
// templated equality functor
template <class T> struct equal;
// templated hashing functor
template <class T> struct hasher;
// @brief combining hashes
template <class T> void hash_combine(std::size_t& seed, const T& v);

/**
 * @brief a state class
 * @class state
 * @version 0.1.1
 * @date 28-June-2017
 *
 * The state class is **desribed** by the template parameter `state_trait`
 * which can be virtually anything, provided you can use it to calculate a hash.
 * You are the one who decides what `state_trait` is depending on your application.
 * The ReLearn library takes a **copy** of the state_trait, so if using pointers,
 * please make sure you understand what those implications are.
 */
template <class state_trait,
          typename value_type = double>
class state
{
public:
    using trait_type = state_trait;
    /// construct without a reward (R set to zero)
    state(state_trait trait);
    /// construct with a reward (terminal state)
    state(value_type reward, state_trait trait);
    /// @brief state equality - uses `state_trait::operator==`
    bool operator==(const state<state_trait> & arg) const;
    /// @brief comparison / osrting operator - uses `state_trait::operator<`
    bool operator<(const state<state_trait> & arg) const;
    /// @return unique hash
    std::size_t hash() const;
    /// @return reward: 0 for normal, -1 for negative, +1 for positive
    value_type reward() const;
    /// @return trait
    state_trait trait() const;
private:
    // state reward
    value_type __reward__;
    // state descriptor (actual object/value)
    state_trait __trait__;
#if USING_BOOST_SERIALIZATION
    friend boost::serialization::access;
    // @warning - template parameter `state_trait` must be serializable
    template <typename archive>
    void serialize(archive & ar, const unsigned int version);
protected:
    state() = default;
#endif
};
// hashing functor for template class state
template <class state_trait> 
struct hasher<state<state_trait>>
{
    std::size_t operator()(const state<state_trait> & arg) const;
};

/**
 * @brief an action class - wraps around your class or pdt
 * @class action
 * @version 0.1.1
 * @date 6-July-2017
 * 
 * In Reinforcement Learning literature, most often an action
 * is an affector which changes the previous state to a new/next state.
 * It must wrap around template parameter `action_trait`.
 * You have to decide what an `action` is in the context of your application,
 * how memory is managed and so on so forth.
 * Do bear in mind that class `policy` keeps a copy of the action(s).
 */
template <class action_trait> 
class action
{
public:
    using trait_type = action_trait;
    /// @brief construct using @param next state
    action(action_trait trait); 
    /// @brief equality operator - uses `action_trait::operator==`
    bool operator==(const action<action_trait> & arg) const;
    /// @brief comparison / sorting operator - uses `action_trait::operator<`
    bool operator<(const action<action_trait> & arg) const;
    /// hashing 
    std::size_t hash() const;
    /// return trait 
    action_trait trait() const;
private:
    /// action descriptor - object/value wrapped
    action_trait __trait__;
#if USING_BOOST_SERIALIZATION
    friend class boost::serialization::access;
    // @warning - template parameter `action_trait` must be serializable
    template <typename archive>
    void serialize(archive & ar, const unsigned int version);
protected:
    action() = default;
#endif
};
//hashing functor for template class action
template <class action_trait> 
struct hasher<action<action_trait>>
{
    std::size_t operator()(const action<action_trait> &arg) const;
};

/**
 * @struct link
 * @brief a simple `link` or pair for joining state-actions in the MDP
 * @date 29-06-2017
 * @version 0.1.0
 */
template <class state_class, 
          class action_class>
struct link
{
    state_class state;
    action_class action;
    /// @brief sort/comparison operator
    bool operator<(const link<state_class,action_class> & arg) const;
    /// @brief equality comparisong operator
    bool operator==(const link<state_class,action_class> & arg) const;
};

/**
 * @brief the class which encapsulates learnt policies, actions and values
 * @class policy
 *
 * Template parameter `state_class` defines the state `s_t`
 * Please note that it must be hashable (@see above class `state`)
 *
 * Template parameter `action_class` defines the action `a_t`
 * Please not that it must be hashabe (@see above class `action`)
 *
 * Template parameter `value_type` defaults to a double, but feel free to change it
 *
 * This class owns all mapped state-action-policy values (it keeps copies)
 * It learns which are better than others, by observing terminal state rewards.
 * The actual **value** is not calculated in this class (it is agnostic in that
 * respect) but is instead calculated using another algorith (Q-Learning, etc.)
 *
 * Use this class in combination with an algorithm to train it (e.g., `q_learning`)
 */
template <class state_class, 
          class action_class,
          typename value_type = double>
class policy
{
public:
    /// @brief action_map correlates a state to its experienced actions/values
    using action_map = std::unordered_map<action_class,
                                          value_type,
                                          hasher<action_class>>;
    /// @return actions experienced for this state
    action_map actions(state_class s_t);
    /// @brief update a policy value
    void update(state_class s_t, 
				action_class a_t, 
				value_type q_value);
    /// @return value of policy
    value_type value(state_class s_t, 
					 action_class a_t);
    /// @return max/best policy for @param state
    value_type best_value(state_class s_t);
    /** 
	 * @return best policy for @param state - 
     * @warning if none are found, returns nullptr
     */
    std::unique_ptr<action_class> best_action(state_class s_t);
private:
// if using boost serialization the policies used
// are wrappers defined in internal header `serialize.tpl`
#if USING_BOOST_SERIALIZATION
    friend class boost::serialization::access;
    template <typename archive>
    void serialize(archive & ar, const unsigned int version);
    std::unordered_map<state_serial
                       std::unordered_map<action_serial,
                                          value_type,
                                          hasher<action_serial>>,
                       hasher<state_serial>
                       > __policies__;
// else we're using the actual `state_class` and `action_class` types
#else
    std::unordered_map<state_class, 
                       std::unordered_map<action_class,
                                          value_type,
                                          hasher<action_class>>,
                       hasher<state_class>
                       > __policies__;
#endif
};
// hash functor for `unordered_map<action_class,value_type`
template <class action_class,
          typename value_type>
struct hasher<std::unordered_map<action_class,value_type>>
{
    std::size_t operator()(const std::unordered_map<action_class,value_type> &arg) const;
};

/**
 * @class q_learning This is the **deterministic** Q-Learning algorithm
 * @brief Q-Learning update algorithm sets policies using episodes (`markov_chain`)
 * @param gamma is the discount rate
 * @param alpha is the learning rate
 *
 * Q(s_t,a_t) = Q(s_t,a_t) + α * (r_t + γ * max(Q(s_{t+1}, a)) - Q(s_t, a_t))
 *
 * or:
 *
 * Q[s,a] = (1 - α) Q[s_t,a_t] + α(r+ γ * max( Q[s_t,a_t]))
 */
template <class state_class, 
          class action_class,
          typename markov_chain = std::deque<link<state_class,action_class>>,
          typename value_type = double> 
struct q_learning
{
    using triplet = std::tuple<state_class, 
                               action_class, 
                               value_type>;
    /// learning rate
    const value_type alpha;
    /// discount rate
    const value_type gamma;
    /// @brief the update rule of Q-learning
    triplet q_value(markov_chain &episode,
                    unsigned int index,
                    policy<state_class,action_class> &policy_map);

    /// @brief do the updating for an episode - @param policy_map will be modified
    void operator()(markov_chain episode, 
                    policy<state_class,action_class> & policy_map);
};

/**
 * @struct q_probabilistic This is the **non-deterministic** Q-Learning algorithm
 * @brief Q-Learning updates policy values using various episodes (`markov_chain`)
 * @param gamma is the discount rate
 *
 * Algorithm uses transition probabilities (e.g., from s_t → s_t+1) mapped by `r`
 * The probability of a transition `r` is `P(s_t,a_t) = Σ P(s_t,a_t)(s_t,s_t+1)`
 * relies on classical probability on the frequency of the transition with respect
 * to the total observations of `P(s_t,a_t)` regardless of transition (the sum).
 * Thus our **expected reward** is `E(r)` is the value of the reward of that
 * transition probability. We do not reward the transition, but the ending state s_t+1
 * thus R = R(s_t+1) and we assume the Reward to be **Stationary** (e.g., non-changing).
 * The Policy Value V(s_t) = Max Q(s_t,a_t), therefore our update rule in combination
 * with the expected reward, is:
 *
 * Q(s_t,a_t) ← E(R(s_t+1)) + γ  Σ maxQ(s_t+1,a_t) P(s_t,a_t)(s_t+1)`
 *
 * or verbalised: the Expected reward of the transitional state (the state we expect to end to)
 * plus the discoun multiplied by the cumulative probability of the transition (s_t → s_t+1) 
 * times the max Q value of the next state Q(s_t+1,a_t).
 */
template <class state_class, 
          class action_class,
          typename markov_chain = std::deque<link<state_class,action_class>>,
          typename value_type = double> 
struct q_probabilistic
{
    // map of state, frequency (s_t_+1)
    using frequency   = std::unordered_map<state_class,
                                           std::size_t,
                                           hasher<state_class>>;
    // frequency of transition map from a_t to (s_t+1)
    using transition  = std::unordered_map<action_class,
                                           frequency,
                                           hasher<frequency>>;
    // frequency of observation of transition (s_t,a_t) → (s_t+1)
    using observation = std::unordered_map<state_class,
                                           transition,
                                           hasher<transition>>;
    // Q-triplet: state,action => value
    using triplet = std::tuple<state_class, 
                       		   action_class, 
                       		   value_type>;
    /// discount rate
    const value_type gamma;

    /// @brief the update rule of Q-learning
    triplet q_value(markov_chain &episode,
                    unsigned int index,
                    policy<state_class,action_class> & policy_map);

    /// @brief do the updating for an episode - @param policy_map will be modified
    void operator()(markov_chain episode, 
                    policy<state_class,action_class> & policy_map);
private:
    observation __memory__;
};
// hashing for `frequency` map
template <class state_class>
struct hasher<std::unordered_map<state_class,std::size_t>>
{
    std::size_t operator()(const std::unordered_map<state_class,
                                                    std::size_t> & arg) const;
};
// hashing for `transition` map
template <class state_class,
          class action_class>
struct hasher<std::unordered_map<action_class,
                std::unordered_map<state_class,
                                   std::size_t>>>
{
    std::size_t operator()(const std::unordered_map<action_class,
                                    std::unordered_map<state_class,
                                                       std::size_t>> & arg) const;
};
// hashing for `observation` map
template <class state_class,
          class action_class>
struct hasher<std::unordered_map<state_class,
                                 std::unordered_map<action_class,
                                    std::unordered_map<state_class,
                                                       std::size_t>>>>
{
    std::size_t operator()(const std::unordered_map<state_class,
                                    std::unordered_map<action_class,
                                        std::unordered_map<state_class,
                                                           std::size_t>>> & arg) const;
};

/********************************************************************************
 ***********               Template Implementations
 ***********                            />                      
 ***********                      _,,  / >
 ***********                     "-=\~ \ > _
 ***********                        \\~___( ~
 ***********                       _|/---\\_
 ***********                       \        \
 ********************************************************************************/

template <class T>
void hash_combine(std::size_t& seed, const T& v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <class action_trait>
std::size_t hasher<action<action_trait
				  >>::operator()(const action<action_trait> &arg) const
{
    return arg.hash();
} 

template <class state_trait>
std::size_t hasher<state<state_trait>
				  >::operator()(const state<state_trait> &arg) const
{
    return arg.hash();
} 

template <class action_class,
          typename value_type>
std::size_t hasher<std::unordered_map<action_class,value_type>
				  >::operator()(const std::unordered_map<action_class,
													     value_type> &arg) const
{
    std::size_t seed;
    for (const auto & pair : arg) {
        hash_combine(seed, pair.first.hash());
    }
    return seed;
}

template <class state_class>
std::size_t 
    hasher<std::unordered_map<state_class,std::size_t>
          >::operator()(const std::unordered_map<state_class,
                                                 std::size_t> & arg) const
{
    std::size_t seed;
    for (const auto & pair : arg) {
        hash_combine(seed, pair.first.hash());
    }
    return seed;
}

template <class state_class,
          class action_class>
std::size_t 
    hasher<std::unordered_map<action_class,
                std::unordered_map<state_class,std::size_t>>
          >::operator()(const std::unordered_map<action_class,
                                    std::unordered_map<state_class,
                                                       std::size_t>> & arg) const
{
    std::size_t seed;
    for (const auto & pair : arg) {
        hash_combine(seed, pair.first.hash());
    }
    return seed;
}

template <class state_class,
          class action_class>
std::size_t 
    hasher<std::unordered_map<state_class,
                std::unordered_map<action_class,
                     std::unordered_map<state_class,std::size_t>>>
         >::operator()(const std::unordered_map<state_class,
                                  std::unordered_map<action_class,
                                       std::unordered_map<state_class,
                                                          std::size_t>>> & arg) const
{
    std::size_t seed;
    for (const auto & pair : arg) {
        hash_combine(seed, pair.first.hash());
    }
    return seed;
}

template <class state_trait,
          typename value_type>
state<state_trait,value_type>::state(state_trait trait)
: __reward__(0), 
  __trait__(trait)
{}

template <class state_trait,
          typename value_type>
state<state_trait,value_type>::state(value_type reward, 
			   state_trait trait)
: __reward__(reward), 
  __trait__(trait)
{}

template <class state_trait,
          typename value_type>
value_type state<state_trait,value_type>::reward() const
{
    return __reward__;
}

template <class state_trait,
          typename value_type>
bool state<state_trait,value_type>::operator==(const state<state_trait> & arg) const
{
    return (this->__trait__ == arg.__trait__);
}

template <class state_trait,
          typename value_type>
bool state<state_trait,value_type>::operator<(const state<state_trait> & arg) const
{
    return (this->__trait__ < arg.__trait__);
}

template <class state_trait,
          typename value_type>
std::size_t state<state_trait,value_type>::hash() const
{
    return std::hash<state_trait>{}(__trait__);
}

template <class state_trait,
          typename value_type>
state_trait state<state_trait,value_type>::trait() const
{
    return __trait__;
}

#if USING_BOOST_SERIALIZATION
template <class state_trait,
          typename value_type>
template <typename archive>
void state<state_trait,value_type>::serialize(archive & ar, 
					                          const unsigned int version)
{
    ar & __trait__;
    ar & __reward__;
}
#endif

template <class action_trait>
action<action_trait>::action(action_trait trait)
: __trait__(trait)
{}

template <class action_trait>
std::size_t action<action_trait>::hash() const
{
   return std::hash<action_trait>{}(__trait__); 
}

template <class action_trait>
bool action<action_trait>::operator==(const action<action_trait> & arg) const
{
    return (this->__trait__ == arg.__trait__);
}

template <class action_trait>
bool action<action_trait>::operator<(const action<action_trait> & arg) const
{
    return (this->__trait__ < arg.__trait__);
}

template <class action_trait>
action_trait action<action_trait>::trait() const
{
    return __trait__;
}

#if USING_BOOST_SERIALIZATION
template <class action_trait>
template <typename archive>
void action<action_trait>::serialize(archive & ar, 
									 const unsigned int version)
{
    ar & __trait__;
}
#endif

template <class state_class, 
          class action_class>
bool link<state_class,action_class>::operator<(const link<state_class,
                                               action_class> & arg) const
{
    return (this->action < arg.action) &&
           (this->state  < arg.state);
}

template <class state_class, 
          class action_class>
bool link<state_class,action_class>::operator==(const link<state_class,
                                                action_class> & arg) const
{
    return (this->action == arg.action) &&
           (this->state  == arg.state);
}

template <class state_class,
          class action_class,
          typename value_type>
typename policy<state_class,action_class,value_type>::action_map 
            policy<state_class,action_class,value_type>::actions(state_class s_t)
{
    return __policies__[s_t];
}

template <class state_class,
          class action_class,
          typename value_type>
void policy<state_class,action_class,value_type>::update(state_class s_t, 
                                                         action_class a_t, 
                                                         value_type q)
{
    __policies__[s_t][a_t] = q;
}

template <class state_class,
          class action_class,
          typename value_type>
value_type policy<state_class,action_class,value_type>::value(state_class s_t, 
                                                              action_class a_t)
{
    return __policies__[s_t][a_t];
}

template <class state_class,
          class action_class,
          typename value_type>
value_type policy<state_class,action_class,value_type>::best_value(state_class s_t)
{
    auto it = std::max_element(__policies__[s_t].begin(), 
                               __policies__[s_t].end(),
              [&](const auto &lhs, const auto &rhs) { return lhs.second < rhs.second; });
    return it != __policies__[s_t].end() ? it->second : 0.;
}

template <class state_class,
          class action_class,
          typename value_type>
std::unique_ptr<action_class> 
		policy<state_class,action_class,value_type>::best_action(state_class s_t)
{
    auto it = std::max_element(__policies__[s_t].begin(), __policies__[s_t].end(),
              [&](const auto &lhs, const auto &rhs) {
                  return lhs.second < rhs.second;
              });
    return it != __policies__[s_t].end() ?
           std::move(std::make_unique<action_class>(it->first)) : nullptr;
}

#if USING_BOOST_SERIALIZATION
template <class state_class,
          class action_class,
          typename value_type>
template <typename archive>
void policy<state_class,action_class,value_type>::serialize(archive & ar, 
                                                            const unsigned int version)
{
    ar & __policies__;
}
#endif

template <class state_class, 
          class action_class,
          typename markov_chain,
          typename value_type>
typename q_learning<state_class,action_class,markov_chain,value_type>::triplet
            q_learning<state_class,action_class,markov_chain,value_type
					  >::q_value(markov_chain & episode, 
					   			 unsigned int index,
								 policy<state_class,action_class> & policy_map) 
{
    auto step = episode[index];
    if (index < episode.size() - 1)  {
        auto q      = policy_map.value(step.state, step.action);
        auto next   = episode[index + 1];
        auto q_next = policy_map.best_value(next.state);
        auto r      = step.state.reward();
        return std::make_tuple(step.state, step.action, 
                               q + alpha * (r + (gamma * q_next) - q));
    }
    else {
        return std::make_tuple(step.state, step.action, step.state.reward());
    }
}

template <class state_class, 
          class action_class,
          typename markov_chain,
          typename value_type>
void q_learning<state_class,action_class,markov_chain,value_type
                >::operator()(markov_chain episode, 
                 			  policy<state_class,action_class> & policy_map)
{
    // @NOTE - maybe replace index with an STL iterator?
    for (unsigned int i = 0; i < episode.size(); i++) {
        auto triplet = q_value(episode, i, policy_map);
        policy_map.update(std::get<0>(triplet), 
                          std::get<1>(triplet), 
                          std::get<2>(triplet));
    }
}

template <class state_class, 
          class action_class,
          typename markov_chain,
          typename value_type>
typename q_probabilistic<state_class,action_class,markov_chain,value_type>::triplet
            q_probabilistic<state_class,action_class,markov_chain,value_type
					       >::q_value(markov_chain & episode, 
                                      unsigned int index,
                                      policy<state_class,action_class> & policy_map) 
{
    auto step = episode[index];
    if (index < episode.size() - 1)  {
        auto q      = policy_map.value(step.state, step.action);
        auto next   = episode[index + 1];
        auto q_next = policy_map.best_value(next.state);
        auto r      = step.state.reward();
        // transition probability (frequency of transition / total observations)
        value_type prob = (__memory__[step.state][step.action][next.state]) /
                          (__memory__[step.state][step.action].size());
        // expected reward
        value_type r_expected = prob * r;
        return std::make_tuple(step.state, step.action, 
                               r_expected + (gamma * (q_next * prob)));
    }
    else {
        return std::make_tuple(step.state, step.action, step.state.reward());
    }

}

template <class state_class, 
          class action_class,
          typename markov_chain,
          typename value_type>
void q_probabilistic<state_class,action_class,markov_chain,value_type
                    >::operator()(markov_chain episode, 
                 			      policy<state_class,action_class> & policy_map)
{
    for (unsigned int i = 0; i < episode.size(); i++) {
        if (i == episode.size() - 1) {
            continue;
        }
        auto s_t = episode[i].state;
        auto a_t = episode[i].action;
        auto s_n = episode[i +1].state;
        __memory__[s_t][a_t][s_n]++;
    }
    for (unsigned int i = 0; i < episode.size(); i++) {
        auto triplet = q_value(episode, i, policy_map);
        policy_map.update(std::get<0>(triplet), 
                          std::get<1>(triplet), 
                          std::get<2>(triplet));
    }
}

} // end of namespace
#endif
