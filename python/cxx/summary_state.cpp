/*
  Copyright 2019 Equinor ASA.

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <chrono>

#include <opm/parser/eclipse/EclipseState/Schedule/SummaryState.hpp>

#include <pybind11/stl.h>
#include <pybind11/chrono.h>
#include "export.hpp"

namespace {


std::vector<std::string> groups(const SummaryState * st) {
    return st->groups();
}

std::vector<std::string> wells(const SummaryState * st) {
    return st->wells();
}


}


void python::common::export_SummaryState(py::module& module) {

    py::class_<SummaryState>(module, "SummaryState")
        .def(py::init<std::chrono::system_clock::time_point>())
        .def("update", &SummaryState::update)
        .def("update_well_var", &SummaryState::update_well_var)
        .def("update_group_var", &SummaryState::update_group_var)
        .def("well_var", &SummaryState::get_well_var)
        .def("group_var", &SummaryState::get_group_var)
        .def("elapsed", &SummaryState::get_elapsed)
        .def_property_readonly("groups", groups)
        .def_property_readonly("wells", wells)
        .def("__getitem__", &SummaryState::get);
}
