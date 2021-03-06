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

 File: cldes/operations/Operations.hpp
 Description: Definition of operation functions: Parallel composition,
 virtual parallel composition and supervisor synthesis.
 =========================================================================
*/

#ifndef OPERATIONS_HPP
#define OPERATIONS_HPP

#include "cldes/Constants.hpp"
#include "cldes/DESystem.hpp"
#include "cldes/operations/SyncSysProxy.hpp"
#include "cldes/operations/SuperProxy.hpp"
#include "cldes/src/operations/OperationsFwd.hpp"

namespace cldes {

namespace op {

template<class SysT_l>
using EventsSet_t = EventsSet<SysTraits<SysT_l>::Ne_>;

template<class SysT_l>
using StatesTableHost_t = StatesTableHost<typename SysTraits<SysT_l>::Si_>;

template<class SysT_l>
using transMap_t = transMap<typename SysTraits<SysT_l>::Si_>;

template<class SysT_l>
using DESystem_t =
  DESystem<SysTraits<SysT_l>::Ne_, typename SysTraits<SysT_l>::Si_>;

/*! \brief Calculate the parallel composition between two systems
 * \details The composed states are sorted by the right operand indexes:
 * e.g. indexes(sys0.size{3} || sys.size{2}) =
 * {0 = (0, 0), 1 = (1, 0), 2 = (2, 0), 3 = (0, 1), 4 = (1, 1), 5 = (2, 1)}
 * \warning synchronize() is faster than the other synchronizing functions to
 * calculate the whole system. However, parallel composition between large
 * systems can occupy a lot of memory. Prefer lazy operations when this
 * is a problem
 *
 * @param aSys0 The left operand of the parallel composition.
 * @param aSys1 The right operand of the parallel composition.
 * \return A concrete system which represents a parallel composition
 * between two systems
 */
template<class SysT_l, class SysT_r>
DESystem_t<SysT_l> constexpr synchronize(SysT_l const& aSys0,
                                         SysT_r const& aSys1) noexcept
{
    uint8_t constexpr NEvents = SysTraits<SysT_l>::Ne_;
    using StorageIndex = typename SysTraits<SysT_l>::Si_;

    DESystem<NEvents, StorageIndex> sys = DESystem<NEvents, StorageIndex>(
      SyncSysProxy<SysT_l, SysT_r>{ aSys0, aSys1 });

    return sys;
}

/*! \brief Lazy evaluation of the parallel composition between two systems
 * \details The composed states are sorted by the right operand indexes:
 * e.g. indexes(sys0.size{3} || sys.size{2}) =
 * {0 = (0, 0), 1 = (1, 0), 2 = (2, 0), 3 = (0, 1), 4 = (1, 1), 5 = (2, 1)}
 * \warning This funtion returns an object that evaluates the operation
 * on demand. If you need the whole system at once, use synchronize or
 * convert with the result to DESystem using a type cast.
 *
 * @param aSys0 The left operand of the parallel composition.
 * @param aSys1 The right operand of the parallel composition.
 * \return A virtual system which represents a parallel composition
 * between two systems.
 */
template<class SysT_l, class SysT_r>
SyncSysProxy<SysT_l, SysT_r> constexpr synchronizeStage1(
  SysT_l const& aSys0,
  SysT_r const& aSys1) noexcept
{
    return SyncSysProxy<SysT_l, SysT_r>{ aSys0, aSys1 };
}

/*! \brief Final stage of the lazy parallel composition evaluation
 * \details transform a virtual proxy in a concrete system. It is implicited
 * called when virtual proxy to the parallel composition operation that
 * has no value calculated is converted to a concrete system.
 *
 * @param[out] aVirtualSys Reference to the system which will be transformed.
 * \return void
 */
template<class SysT_l, class SysT_r>
void
synchronizeEmptyStage2(SyncSysProxy<SysT_l, SysT_r>& aVirtualSys,
                       SysT_l,
                       SysT_r) noexcept;


/*! \brief Remove bad states recursively
 * \details Remove a state and all the states that become a bad state when the
 * previous one is removed.
 *
 * @param[out] aVirtualSys reference to the virtual system which will have a
 * state removed
 * @param aC A hash table containing the states currently added to the
 * virtual system.
 * @param aQ The state to be removed.
 * @param aNonContrBit Bit array containing the virtual system non controllable
 * events.
 * @param aRmTable A hash table containing all the removed states so far
 * \return void
 */
template<class SysT_l, class SysT_r, class StTabT, typename StorageIndex>
inline void
removeBadStates_(SyncSysProxy<SysT_l, SysT_r> const& aVirtualSys,
                 StTabT& aC,
                 StorageIndex const& aQ,
                 EventsSet_t<SysT_l> const& aNonContrBit,
                 StatesTableHost_t<SysT_l>& aRmTable) noexcept;

/*! \brief Computes the monolithic supervisor of a plant and a spec
 *
 * @param aP Plant system const reference
 * @param aE Specs system const reference
 * @param aNonContr Hash table containing all non-controllable events indexes.
 * \return The monolithic supervisorconcrete system
 */
template<class SysT_l, class SysT_r>
DESystem<SysTraits<SysT_l>::Ne_, typename SysTraits<SysT_l>::Si_>
supC(SysT_l const& aP,
     SysT_r const& aE,
     EventsTableHost const& aNonContr) noexcept;

/*! \brief Computes the monolithic supervisor of plants and specs
 * \details Build a binary expression tree of synchronizations and execute
 * the supervisor synthesis with the second level of the tree.
 * The root of the tree is the supervisor.
 * \warning Assumes that vectors has length >= 2
 * \note It balance the trees with a naive algorithm. The most
 * effiecient this structure is, the most scattered the events are
 * on the leafs: Not only the balance matters here.
 *
 * @param aPlants Vector containing plants systems const references
 * @param aSpecs Vector containing specs systems const references
 * @param aNonContr Hash table containing all non-controllable events indexes
 * \return The monolithic supervisor concrete system
 */
// template<uint8_t NEvents, typename StorageIndex>
// DESystem<NEvents, StorageIndex>
// supC(DESVector<NEvents, StorageIndex> const& aPlants,
//                 DESVector<NEvents, StorageIndex> const& aSpecs,
//                 EventsTableHost const& aNonContr);

} // namespace op
} // namespace cldes

// include functions definitions
#include "cldes/src/operations/OperationsCore.hpp"

#endif // OPERATIONS_HPP
