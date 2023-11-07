/*
  Copyright 2020 Statoil ASA.

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

#include <opm/common/OpmLog/OpmLog.hpp>
#include <opm/common/utility/OpmInputError.hpp>

#include <opm/input/eclipse/Parser/ParserKeywords/U.hpp>

#include <opm/input/eclipse/Schedule/HandlerContext.hpp>
#include <opm/input/eclipse/Schedule/MSW/SegmentMatcher.hpp>
#include <opm/input/eclipse/Schedule/ScheduleState.hpp>
#include <opm/input/eclipse/Schedule/UDQ/UDQConfig.hpp>

#include <fmt/format.h>

namespace Opm {

namespace {

void handleUDQ(HandlerContext& handlerContext)
{
    auto new_udq = handlerContext.state().udq();

    auto segment_matcher_factory = [&handlerContext]()
    {
        return std::make_unique<SegmentMatcher>(handlerContext.state());
    };

    for (const auto& record : handlerContext.keyword) {
        new_udq.add_record(segment_matcher_factory, record,
                           handlerContext.keyword.location(),
                           handlerContext.currentStep);
    }

    handlerContext.state().udq.update(std::move(new_udq));
}

void handleUDT(HandlerContext& handlerContext)
{
    auto new_udq = handlerContext.state().udq();

    using PUDT = ParserKeywords::UDT;

    const auto& header = handlerContext.keyword.getRecord(0);
    const std::string name = header.getItem<PUDT::TABLE_NAME>().get<std::string>(0);

    const int dim = header.getItem<PUDT::DIMENSIONS>().get<int>(0);
    if (dim != 1) {
        throw OpmInputError("Only 1D UDTs are supported",
                            handlerContext.keyword.location());

    }

    const auto& points = handlerContext.keyword.getRecord(1);
    const std::string interp_type = points.getItem<PUDT::INTERPOLATION_TYPE>().get<std::string>(0);
    UDT::InterpolationType type;
    if (interp_type == "NV") {
        type = UDT::InterpolationType::NearestNeighbour;
    } else if (interp_type == "LC") {
        type = UDT::InterpolationType::LinearClamp;
    } else if (interp_type == "LL") {
        type = UDT::InterpolationType::LinearExtrapolate;
    } else {
        throw OpmInputError(fmt::format("Unknown UDT interpolation type {}", interp_type),
                            handlerContext.keyword.location());
    }
    const auto x_vals = points.getItem<ParserKeywords::UDT::INTERPOLATION_POINTS>().getData<double>();

    if (!std::is_sorted(x_vals.begin(), x_vals.end())) {
        throw OpmInputError("UDT: Interpolation points need to be given in ascending order",
                            handlerContext.keyword.location());
    }

    if (auto it = std::adjacent_find(x_vals.begin(), x_vals.end()); it != x_vals.end()) {
        throw OpmInputError(fmt::format("UDT: Interpolation points need to be unique: "
                                        "found duplicate for {}", *it),
                            handlerContext.keyword.location());
    }

    const auto& data = handlerContext.keyword.getRecord(2);
    const auto y_vals = data.getItem<ParserKeywords::UDT::TABLE_VALUES>().getData<double>();

    if (x_vals.size() != y_vals.size()) {
        throw OpmInputError(fmt::format("UDT data size mismatch, number of x-values {}",
                                        ", number of y-values {}",
                                        x_vals.size(), y_vals.size()),
                            handlerContext.keyword.location());
    }

    new_udq.add_table(name, UDT(x_vals, y_vals, type));

    handlerContext.state().udq.update(std::move(new_udq));
}

}

bool handleUDQKeyword(HandlerContext& handlerContext)
{
    // handlers that do not need access to schedule members
    using handler_function = std::function<void(HandlerContext&)>;
    static const std::unordered_map<std::string, handler_function> handler_functions = {
        { "UDQ", &handleUDQ},
        { "UDT", &handleUDT},
    };

    auto function_iterator = handler_functions.find(handlerContext.keyword.name());
    if (function_iterator == handler_functions.end()) {
        return false;
    }

    try {
        function_iterator->second(handlerContext);
    } catch (const OpmInputError&) {
        throw;
    } catch (const std::logic_error& e) {
        // Rethrow as OpmInputError to provide more context,
        // but add "Internal error: " to the string, as that
        // is what logic_error signifies.
        const OpmInputError opm_error { std::string("Internal error: ") + e.what(), handlerContext.keyword.location() } ;
        OpmLog::error(opm_error.what());
        std::throw_with_nested(opm_error);
    } catch (const std::exception& e) {
        // Rethrow as OpmInputError to provide more context.
        const OpmInputError opm_error { e, handlerContext.keyword.location() } ;
        OpmLog::error(opm_error.what());
        std::throw_with_nested(opm_error);
    }

    return true;
}

}
