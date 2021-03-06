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

 File: tests/basics.cpp
 Description: Exemplify the basic usage of the library.
 =========================================================================
*/

#include "cldes/DESystem.hpp"
#include "cldes/DESystemCL.hpp"
#include <iostream>
#include <set>
#include <string>

#include "testlib.hpp"

int
main()
{
    std::cout << "Creating DES" << std::endl;
    int const n_states = 4;

    cldes::DESystem<3>::StatesSet marked_states;
    marked_states.insert(0);
    marked_states.insert(2);

    int const init_state = 0;

    cldes::DESystem<3> sys{ n_states, init_state, marked_states };

    // Declare transitions: represented by prime numbers
    cldes::ScalarType const a = 0;
    cldes::ScalarType const b = 1;
    cldes::ScalarType const g = 2;

    sys(0, 0) = a;
    sys(0, 2) = g;
    sys(1, 0) = a;
    sys(1, 1) = b;
    sys(2, 1) = a;
    sys(2, 1) = g;
    sys(2, 2) = b;
    sys(2, 3) = a;

    cldes::DESystemCL<3> sys_gpu{ sys };

    auto accessible_states = sys_gpu.accessiblePart();

    ProcessResult(accessible_states, "< Accessible part", "0 1 2 3 >");

    auto coaccessible_states = sys_gpu.coaccessiblePart();

    ProcessResult(coaccessible_states, "< Coaccessible part", "0 1 2 >");
    std::cout << "Creating new system" << std::endl;

    cldes::DESystem<3> new_sys{ n_states, init_state, marked_states };

    // This graph has no transition from the 3rd state to th 4th one.
    new_sys(0, 0) = a;
    new_sys(0, 2) = g;
    new_sys(1, 1) = b;
    new_sys(2, 1) = a;
    new_sys(2, 1) = g;
    new_sys(2, 2) = b;
    new_sys(3, 1) = a;
    new_sys(3, 2) = a;

    cldes::DESystemCL<3> new_sys_gpu{ new_sys };

    auto new_accessible_states = new_sys_gpu.accessiblePart();

    ProcessResult(new_accessible_states, "< Accessible part", "0 1 2 >");

    auto new_coaccessible_states = new_sys_gpu.coaccessiblePart();

    ProcessResult(new_coaccessible_states, "< Coaccessible part", "0 2 3 >");

    return 0;
}
