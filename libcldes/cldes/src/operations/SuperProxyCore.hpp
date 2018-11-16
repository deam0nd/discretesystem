/*
 =========================================================================
 This file is part of clDES

 clDES: an OpenCL library for Discrete Event Systems computing.

 clDES is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 clDES is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with clDES.  If not, see <http://www.gnu.org/licenses/>.

 Copyright (c) 2018 - Adriano Mourao <adrianomourao@protonmail.com>
                      madc0ww @ [https://github.com/madc0ww]

 LacSED - Laboratorio de Analise e Controle de Sistemas a Eventos Discretos
 Universidade Federal de Minas Gerais

 File: cldes/src/operations/SuperProxyCore.hpp
 Description: SuperProxy methods definitions
 =========================================================================
*/
/*!
 * \file cldes/src/operations/SuperProxyCore.hpp
 *
 * \author Adriano Mourao \@madc0ww
 * \date 2018-06-16
 *
 * Virtual Proxy for the monolithic supervisor synthesis.
 */

namespace cldes {
template<class SysT_l, class SysT_r>
op::SuperProxy<SysT_l, SysT_r>::SuperProxy(SysT_l const& aPlant,
                                           SysT_r const& aSpec,
                                           EventsTableHost const& aNonContr)
  : Base{ aPlant.getStatesNumber() * aSpec.getStatesNumber(),
          aSpec.getInitialState() * aPlant.getStatesNumber() +
            aPlant.getInitialState() }
  , sys0_{ aPlant }
  , sys1_{ aSpec }
{
    n_states_sys0_ = aPlant.getStatesNumber();

    auto const in_both = aPlant.getEvents() & aSpec.getEvents();
    only_in_plant_ = aPlant.getEvents() ^ in_both;
    only_in_spec_ = aSpec.getEvents() ^ in_both;
    this->events_ = aPlant.getEvents() | aSpec.getEvents();

    for (auto q0 : aPlant.getMarkedStates()) {
        for (auto q1 : aSpec.getMarkedStates()) {
            this->marked_states_.emplace(q1 * n_states_sys0_ + q0);
        }
    }
    findRemovedStates_(aPlant, aSpec, aNonContr);
}

// template<class SysT_l, class SysT_r>
// op::SuperProxy<SysT_l, SysT_r>::SuperProxy(
//   DESVector<NEvents, StorageIndex> const& aPlants,
//   DESVector<NEvents, StorageIndex> const& aSpecs,
//   EventsTableHost const& aNonContr)
// {
//     // TODO: Implement this function
// }

template<class SysT_l, class SysT_r>
void
op::SuperProxy<SysT_l, SysT_r>::findRemovedStates_(
  SysT_l const& aP,
  SysT_r const& aE,
  EventsTableHost const& aNonContr) noexcept
{
    SyncSysProxy<SysT_l, SysT_r> virtualsys{ aP, aE };
    EventsSet<NEvents> non_contr_bit;
    EventsSet<NEvents> p_non_contr_bit;
    for (cldes::ScalarType event : aNonContr) {
        if (aP.getEvents().test(event)) {
            p_non_contr_bit.set(event);
            if (virtualsys.events_.test(event)) {
                non_contr_bit.set(event);
            }
        }
    }
    StatesTableHost<StorageIndex> rmtable;
    StatesStack<StorageIndex> f;
    f.push(virtualsys.init_state_);
    virtualsys.allocateInvertedGraph();
    while (!f.empty()) {
        auto const q = f.top();
        f.pop();
        if (!rmtable.contains(q) && !virtual_states_.contains(q)) {
            auto const qx = q % virtualsys.n_states_sys0_;
            auto const q_events = virtualsys.getStateEvents(q);
            auto const in_ncqx = p_non_contr_bit & aP.getStateEvents(qx);
            auto const in_ncqx_and_q = in_ncqx & q_events;
            if (in_ncqx_and_q != in_ncqx) {
                removeBadStates_(
                  virtualsys, virtual_states_, q, non_contr_bit, rmtable);
            } else {
                virtual_states_.insert(q);
                cldes::ScalarType event = 0;
                auto event_it = q_events;
                while (event_it.any()) {
                    if (event_it.test(0)) {
                        auto const fsqe = virtualsys.trans(q, event);
                        if (!rmtable.contains(fsqe)) {
                            if (!virtual_states_.contains(fsqe)) {
                                f.push(fsqe);
                            }
                        }
                    }
                    ++event;
                    event_it >>= 1;
                }
            }
        }
    }
    rmtable.clear();
    this->states_number_ = virtual_states_.size();
    trim();
    virtualsys.clearInvertedGraph();
    return;
}

template<class SysT_l, class SysT_r>
op::SuperProxy<SysT_l, SysT_r>::operator RealSys() noexcept
{
    std::sort(virtual_states_.begin(), virtual_states_.end());
    synchronizeStage2(*this);

    auto sys_ptr = std::make_shared<RealSys>(RealSys{});

    sys_ptr->states_number_ = std::move(this->states_number_);
    sys_ptr->init_state_ = std::move(this->init_state_);
    sys_ptr->marked_states_ = std::move(this->marked_states_);
    sys_ptr->states_events_ = std::move(this->states_events_);
    sys_ptr->inv_states_events_ = std::move(this->inv_states_events_);
    sys_ptr->events_ = std::move(this->events_);

    sys_ptr->graph_.resize(this->states_number_, this->states_number_);
    sys_ptr->bit_graph_.resize(this->states_number_, this->states_number_);
    sys_ptr->graph_.setFromTriplets(triplet_.begin(), triplet_.end());
    triplet_.clear();
    sys_ptr->graph_.makeCompressed();
    sys_ptr->bit_graph_.makeCompressed();

    return *sys_ptr;
}

template<class SysT_l, class SysT_r>
bool
op::SuperProxy<SysT_l, SysT_r>::containstrans_impl(
  StorageIndex const& aQ,
  ScalarType const& aEvent) const noexcept
{
    if (!this->virtual_states_.contains(aQ) || !this->events_.test(aEvent)) {
        return false;
    }
    // q = (qx, qy)
    auto const qx = aQ % n_states_sys0_;
    auto const qy = aQ / n_states_sys0_;
    auto const in_x = sys0_.containstrans(qx, aEvent);
    auto const in_y = sys1_.containstrans(qy, aEvent);
    auto contains = false;
    if ((in_x && in_y) || (in_x && only_in_plant_.test(aEvent)) ||
        (in_y && only_in_spec_.test(aEvent))) {
        contains = true;
    }
    return contains;
}

template<class SysT_l, class SysT_r>
typename op::SuperProxy<SysT_l, SysT_r>::StorageIndexSigned
op::SuperProxy<SysT_l, SysT_r>::trans_impl(StorageIndex const& aQ,
                                           ScalarType const& aEvent) const
  noexcept
{
    if (!this->virtual_states_.contains(aQ) || !this->events_.test(aEvent)) {
        return -1;
    }
    // q = (qx, qy)
    auto const qx = aQ % n_states_sys0_;
    auto const qy = aQ / n_states_sys0_;
    auto const in_x = sys0_.containstrans(qx, aEvent);
    auto const in_y = sys1_.containstrans(qy, aEvent);
    if (!((in_x && in_y) || (in_x && only_in_plant_.test(aEvent)) ||
          (in_y && only_in_spec_.test(aEvent)))) {
        return -1;
    }
    if (in_x && in_y) {
        auto const q0 = sys0_.trans(qx, aEvent);
        auto const q1 = sys1_.trans(qy, aEvent);

        return q1 * n_states_sys0_ + q0;
    } else if (in_x) {
        auto const q0 = sys0_.trans(qx, aEvent);
        return qy * n_states_sys0_ + q0;
    } else { // in_y
        auto const q1 = sys1_.trans(qy, aEvent);
        return q1 * n_states_sys0_ + qx;
    }
}

template<class SysT_l, class SysT_r>
bool
op::SuperProxy<SysT_l, SysT_r>::containsinvtrans_impl(
  StorageIndex const& aQ,
  ScalarType const& aEvent) const
{
    if (!this->virtual_states_.contains(aQ) || !this->events_.test(aEvent)) {
        return false;
    }
    // q = (qx, qy)
    auto const qx = aQ % n_states_sys0_;
    auto const qy = aQ / n_states_sys0_;
    auto const in_x = sys0_.containsinvtrans(qx, aEvent);
    auto const in_y = sys1_.containsinvtrans(qy, aEvent);
    auto contains = false;
    if ((in_x && in_y) || (in_x && only_in_plant_.test(aEvent)) ||
        (in_y && only_in_spec_.test(aEvent))) {
        contains = true;
    }
    return contains;
}

template<class SysT_l, class SysT_r>
StatesArray<typename op::SuperProxy<SysT_l, SysT_r>::StorageIndex>
op::SuperProxy<SysT_l, SysT_r>::invtrans_impl(StorageIndex const& aQ,
                                              ScalarType const& aEvent) const
{
    StatesArray<StorageIndex> inv_transitions;
    if (!this->virtual_states_.contains(aQ) || !this->events_.test(aEvent)) {
        return inv_transitions;
    }
    // q = (qx, qy)
    auto const qx = aQ % n_states_sys0_;
    auto const qy = aQ / n_states_sys0_;
    auto const in_x = sys0_.containsinvtrans(qx, aEvent);
    auto const in_y = sys1_.containsinvtrans(qy, aEvent);
    if (!((in_x && in_y) || (in_x && only_in_plant_.test(aEvent)) ||
          (in_y && only_in_spec_.test(aEvent)))) {
        return inv_transitions;
    }
    if (in_x && in_y) {
        auto const inv_trans_0 = sys0_.invtrans(qx, aEvent);
        auto const inv_trans_1 = sys1_.invtrans(qy, aEvent);
        inv_transitions.reserve(inv_trans_0.size() + inv_trans_1.size());
        for (auto q0 : inv_trans_0) {
            for (auto q1 : inv_trans_1) {
                auto const q_from = q1 * n_states_sys0_ + q0;
                inv_transitions.push_back(q_from);
            }
        }
    } else if (in_x) {
        auto const inv_trans_0 = sys0_.invtrans(qx, aEvent);
        inv_transitions.reserve(inv_trans_0.size());
        for (auto q : inv_trans_0) {
            auto const q_from = qy * n_states_sys0_ + q;
            inv_transitions.push_back(q_from);
        }
    } else { // in_y
        auto const inv_trans_1 = sys1_.invtrans(qy, aEvent);
        inv_transitions.reserve(inv_trans_1.size());
        for (auto q : inv_trans_1) {
            auto const q_from = q * n_states_sys0_ + qx;
            inv_transitions.push_back(q_from);
        }
    }
    return inv_transitions;
}

template<class SysT_l, class SysT_r>
typename op::SuperProxy<SysT_l, SysT_r>::EventsSet_t
op::SuperProxy<SysT_l, SysT_r>::getStateEvents_impl(
  StorageIndex const& aQ) const noexcept
{
    auto const state_event_0 = sys0_.getStateEvents(aQ % n_states_sys0_);
    auto const state_event_1 = sys1_.getStateEvents(aQ / n_states_sys0_);
    auto const state_event = (state_event_0 & state_event_1) |
                             (state_event_0 & only_in_plant_) |
                             (state_event_1 & only_in_spec_);
    return state_event;
}

template<class SysT_l, class SysT_r>
typename op::SuperProxy<SysT_l, SysT_r>::EventsSet_t
op::SuperProxy<SysT_l, SysT_r>::getInvStateEvents_impl(
  StorageIndex const& aQ) const
{
    auto const state_event_0 = sys0_.getInvStateEvents(aQ % n_states_sys0_);
    auto const state_event_1 = sys1_.getInvStateEvents(aQ / n_states_sys0_);
    auto const state_event = (state_event_0 & state_event_1) |
                             (state_event_0 & only_in_plant_) |
                             (state_event_1 & only_in_spec_);
    return state_event;
}

template<class SysT_l, class SysT_r>
void
op::SuperProxy<SysT_l, SysT_r>::allocateInvertedGraph_impl() const noexcept
{
    sys0_.allocateInvertedGraph();
    sys1_.allocateInvertedGraph();
}

template<class SysT_l, class SysT_r>
void
op::SuperProxy<SysT_l, SysT_r>::clearInvertedGraph_impl() const noexcept
{
    sys0_.clearInvertedGraph();
    sys1_.clearInvertedGraph();
}

template<class SysT_l, class SysT_r>
void
op::SuperProxy<SysT_l, SysT_r>::trim() noexcept
{
    StatesTableHost<StorageIndex> trimmed_virtual_states;
    for (auto mstate : this->marked_states_) {
        StatesStack<StorageIndex> f;
        f.push(mstate);
        while (!f.empty()) {
            auto const q = f.top();
            f.pop();
            trimmed_virtual_states.insert(q);
            auto const q_events = this->getInvStateEvents(q);
            cldes::ScalarType event = 0;
            auto event_it = q_events;
            while (event_it.any()) {
                if (event_it.test(0)) {
                    auto const fsqelist = this->invtrans(q, event);
                    for (auto fsqe : fsqelist) {
                        if (virtual_states_.contains(fsqe)) {
                            if (!trimmed_virtual_states.contains(fsqe)) {
                                f.push(fsqe);
                            }
                        }
                    }
                }
                ++event;
                event_it >>= 1;
            }
        }
    }
    virtual_states_ = trimmed_virtual_states;
    this->states_number_ = virtual_states_.size();
    return;
}
}
