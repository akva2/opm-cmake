/*
  Copyright 2013 Statoil ASA.

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

#include <opm/input/eclipse/Schedule/HandlerContext.hpp>

#include <opm/common/OpmLog/OpmLog.hpp>
#include <opm/common/utility/OpmInputError.hpp>

#include <opm/input/eclipse/Deck/DeckKeyword.hpp>

#include <opm/input/eclipse/Parser/ParseContext.hpp>

#include <opm/input/eclipse/Schedule/Action/SimulatorUpdate.hpp>

#include <fmt/format.h>

namespace Opm {

void HandlerContext::affected_well(const std::string& well_name)
{
    if (this->sim_update)
        this->sim_update->affected_wells.insert(well_name);
}

void HandlerContext::record_well_structure_change()
{
    if (this->sim_update != nullptr) {
        this->sim_update->well_structure_changed = true;
    }
}

void HandlerContext::welsegs_handled(const std::string& well_name)
{
    if (welsegs_wells)
        welsegs_wells->insert({well_name, keyword.location()});
}

void HandlerContext::compsegs_handled(const std::string& well_name)
{
    if (compsegs_wells)
        compsegs_wells->insert(well_name);
}

void HandlerContext::invalidNamePattern(const std::string& namePattern) const
{
    std::string msg_fmt = fmt::format("No wells/groups match the pattern: \'{}\'", namePattern);
    if (namePattern == "?") {
        /*
          In particular when an ACTIONX keyword is called via PYACTION
          coming in here with an empty list of matching wells is not
          entirely unheard of. It is probably not what the user wanted and
          we give a warning, but the simulation continues.
        */
        auto msg = OpmInputError::format("No matching wells for ACTIONX {keyword}"
                                         "in {file} line {line}.", keyword.location());
        OpmLog::warning(msg);
    } else
        parseContext.handleError(ParseContext::SCHEDULE_INVALID_NAME,
                                 msg_fmt, keyword.location(), errors);
}

}
