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

 LacSED - Laboratório de Sistemas a Eventos Discretos
 Universidade Federal de Minas Gerais

 File: cldes/operations/SyncSysProxy.hpp
 Description: Virtual Proxy for multiple parallel compositions
 =========================================================================
*/

#ifndef SYNC_SYS_PROXY_HPP
#define SYNC_SYS_PROXY_HPP

#include "cldes/Constants.hpp"
#include "cldes/DESystemBase.hpp"
#include "cldes/EventsSet.hpp"

namespace cldes {
namespace op {

// Forward declaration of friend function
template<uint8_t NEvents, typename StorageIndex>
void
SynchronizeStage2(SyncSysProxy<NEvents, StorageIndex>& aVirtualSys);

// Alias to events hash map
using EventsTableHost = spp::sparse_hash_set<uint8_t>;

// Forward declaration of friend function
template<uint8_t NEvents, typename StorageIndex>
DESystem<NEvents, StorageIndex>
SupervisorSynth(DESystem<NEvents, StorageIndex> const& aP,
                DESystem<NEvents, StorageIndex> const& aE,
                EventsTableHost const& aNonContr);

// Remove bad states recursively
/*! \brief Proxy to a virtual sync sys
 *
 * @param NEvents Number of events
 * @param StorageIndex Unsigned type for storing the indexes of each state
 */
template<uint8_t NEvents, typename StorageIndex>
class SyncSysProxy : public DESystemBase<NEvents, StorageIndex>
{
public:
    using StorageIndexSigned = typename std::make_signed<StorageIndex>::type;

    /*! \brief Vector of states type
     */
    using StatesTable =
      typename DESystemBase<NEvents, StorageIndex>::StatesTable;

    /*! \brief Vector of inverted transitions
     *
     * f(s, e) = s_out -> (s_out, (s, e)) is the inverted transition.
     */
    using TrVector =
      std::vector<std::pair<StorageIndex, InvArgTrans<StorageIndex>*>>;

    /*! \brief SyncSysProxy unique constructor
     *
     * Feed const data-members.
     *
     * @param aSys0Ptr Left operand DESystem reference
     * @param aSys1Ptr Right operand DESystem reference
     */
    explicit SyncSysProxy(DESystemBase<NEvents, StorageIndex> const& aSys0,
                          DESystemBase<NEvents, StorageIndex> const& aSys1);

    /*! \brief DESystem destructor
     */
    virtual ~SyncSysProxy() = default;

    /*! \brief Move constructor
     *
     * Enable move semantics
     */
    //SyncSysProxy(SyncSysProxy&&) = default;

    ///*! \brief Copy constructor
    // *
    // * Needs to define this, since move semantics is enabled
    // */
    //SyncSysProxy(SyncSysProxy const&) = default;

    ///*! \brief Operator =
    // *
    // * Uses move semantics
    // */
    //SyncSysProxy<NEvents, StorageIndex>& operator=(SyncSysProxy&&) = default;

    ///*! \brief Operator = to const type
    // *
    // * Needs to define this, since move semantics is enabled
    // */
    //SyncSysProxy<NEvents, StorageIndex>& operator=(SyncSysProxy const&) =
    //  default;

    /*! \brief Overload conversion to DESystem
     *
     */
    explicit operator DESystem<NEvents, StorageIndex>();

    /*! \brief Returns true if DES transition exists
     *
     * @param aQ State
     * @param aEvent Event
     */
    bool ContainsTrans(StorageIndex const& aQ,
                       ScalarType const& aEvent) const override;

    /*! \brief Returns DES transition: q_to = f(q, e)
     *
     * @param aQ State
     * @param aEvent Event
     */
    StorageIndexSigned Trans(StorageIndex const& aQ,
                             ScalarType const& aEvent) const override;

    /*! \brief Returns true if DES inverse transition exists
     *
     * @param aQfrom State
     * @param aEvent Event
     */
    bool ContainsInvTrans(StorageIndex const& aQ,
                          ScalarType const& aEvent) const override;

    /*! \brief Returns DES inverse transition: q = f^-1(q_to, e)
     *
     * @param aQfrom State
     * @param aEvent Event
     */
    StatesArray<StorageIndex> InvTrans(StorageIndex const& aQfrom,
                                       ScalarType const& aEvent) const override;

    /*! \brief Returns EventsSet relative to state q
     *
     * @param aQ A state on the sys
     */
    EventsSet<NEvents> GetStateEvents(StorageIndex const& aQ) const override;

    /*! \brief Returns EventsSet relative to state inv q
     *
     * @param aQ A state on the sys
     */
    EventsSet<NEvents> GetInvStateEvents(StorageIndex const& aQ) const override;

    /*! \brief Invert graph
     *
     * This is used on some operations... it can be very inneficient for very
     * large graphs
     * It is const, since it changes only a mutable member
     */
    inline void AllocateInvertedGraph() const override
    {
        sys0_.AllocateInvertedGraph();
        sys1_.AllocateInvertedGraph();
    }

    /*! \brief Free inverted graph
     *
     * It is const, since it changes only a mutable member
     */
    inline void ClearInvertedGraph() const override
    {
        sys0_.ClearInvertedGraph();
        sys1_.ClearInvertedGraph();
    }

protected:
    /*
     * Friend function
     * Second step of the lazy parallel composition
     */
    friend void cldes::op::SynchronizeStage2<>(
      SyncSysProxy<NEvents, StorageIndex>& aVirtualSys);

    /*
     * Friend function
     * Monolithic supervisor synthesis
     */
    friend DESystem<NEvents, StorageIndex> SupervisorSynth<>(
      DESystem<NEvents, StorageIndex> const& aP,
      DESystem<NEvents, StorageIndex> const& aE,
      EventsTableHost const& aNonContr);

    // Calculate f(q, event) of a virtual system
    /*! \brief Disabled default constructor
     *
     * There is no use for the default constructor.
     */
    explicit SyncSysProxy();

private:
    /*! \brief Raw pointer to DESystemBase object
     *
     * Raw pointer to the owner of the proxied element.
     */
    DESystemBase<NEvents, StorageIndex> const& sys0_;

    /*! \brief Raw pointer to DESystemBase object
     *
     * Raw pointer to the owner of the proxied element.
     */
    DESystemBase<NEvents, StorageIndex> const& sys1_;

    /*! \brief Cache number of states of Sys0
     *
     */
    StorageIndex n_states_sys0_;

    /*! \brief Virtual states contained in the current system
     *
     */
    StatesTable virtual_states_;

    /*! \brief Events contained only in the left operator of a synchronizing op.
     *
     */
    EventsSet<NEvents> only_in_0_;

    /*! \brief Events contained only in the right operator of a synchronizing
     * op.
     *
     */
    EventsSet<NEvents> only_in_1_;

    /*! \brief Events contained only in the right operator of a synchronizing
     * op.
     *
     */
    TrVector transtriplet_;

    /*! \brief 3-tuples for filling graph_
     */
    std::vector<Triplet<NEvents>> triplet_;

    /*! \brief 3-tuples for filling bit_graph_
     */
    std::vector<BitTriplet> bittriplet_;
};

} // namespace op
} // namespace cldes

// including implementation
#include "cldes/src/operations/SyncSysProxyCore.hpp"

#endif // TRANSITION_PROXY_HPP
