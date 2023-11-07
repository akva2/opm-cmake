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
#include <opm/common/utility/String.hpp>

#include <opm/input/eclipse/Deck/DeckKeyword.hpp>

#include <opm/input/eclipse/Parser/ParseContext.hpp>
#include <opm/input/eclipse/Parser/ParserKeywords/W.hpp>

#include <opm/input/eclipse/Schedule/Action/WGNames.hpp>
#include <opm/input/eclipse/Schedule/GasLiftOpt.hpp>
#include <opm/input/eclipse/Schedule/Group/GuideRateConfig.hpp>
#include <opm/input/eclipse/Schedule/HandlerContext.hpp>
#include <opm/input/eclipse/Schedule/RFTConfig.hpp>
#include <opm/input/eclipse/Schedule/ScheduleState.hpp>
#include <opm/input/eclipse/Schedule/UDQ/UDQActive.hpp>
#include <opm/input/eclipse/Schedule/UDQ/UDQConfig.hpp>
#include <opm/input/eclipse/Schedule/Well/NameOrder.hpp>
#include <opm/input/eclipse/Schedule/Well/WDFAC.hpp>
#include <opm/input/eclipse/Schedule/Well/Well.hpp>
#include <opm/input/eclipse/Schedule/Well/WellBrineProperties.hpp>
#include <opm/input/eclipse/Schedule/Well/WellConnections.hpp>
#include <opm/input/eclipse/Schedule/Well/WellEconProductionLimits.hpp>
#include <opm/input/eclipse/Schedule/Well/WellFoamProperties.hpp>
#include <opm/input/eclipse/Schedule/Well/WellMICPProperties.hpp>
#include <opm/input/eclipse/Schedule/Well/WellPolymerProperties.hpp>
#include <opm/input/eclipse/Schedule/Well/WellTestConfig.hpp>
#include <opm/input/eclipse/Schedule/Well/WellTracerProperties.hpp>
#include <opm/input/eclipse/Schedule/Well/WListManager.hpp>
#include <opm/input/eclipse/Schedule/Well/WVFPDP.hpp>
#include <opm/input/eclipse/Schedule/Well/WVFPEXP.hpp>

#include <fmt/format.h>

#include <unordered_set>

namespace Opm {

namespace {

/*
  The function trim_wgname() is used to trim the leading and trailing spaces
  away from the group and well arguments given in the WELSPECS and GRUPTREE
  keywords. If the deck argument contains a leading or trailing space that is
  treated as an input error, and the action taken is regulated by the setting
  ParseContext::PARSE_WGNAME_SPACE.

  Observe that the spaces are trimmed *unconditionally* - i.e. if the
  ParseContext::PARSE_WGNAME_SPACE setting is set to InputError::IGNORE that
  means that we do not inform the user about "our fix", but it is *not* possible
  to configure the parser to leave the spaces intact.
*/
std::string trim_wgname(const DeckKeyword& keyword,
                        const std::string& wgname_arg,
                        const ParseContext& parseContext,
                        ErrorGuard& errors)
{
    std::string wgname = trim_copy(wgname_arg);
    if (wgname != wgname_arg)  {
        const auto& location = keyword.location();
        std::string msg_fmt = fmt::format("Problem with keyword {{keyword}}\n"
                                          "In {{file}} line {{line}}\n"
                                          "Illegal space in {} when defining WELL/GROUP.", wgname_arg);
        parseContext.handleError(ParseContext::PARSE_WGNAME_SPACE, msg_fmt, location, errors);
    }
    return wgname;
}

void handleCOMPDAT(HandlerContext& handlerContext)
{
    std::unordered_set<std::string> wells;
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        auto wellnames = handlerContext.wellNames(wellNamePattern);

        for (const auto& name : wellnames) {
            auto well2 = handlerContext.state().wells.get(name);
            auto connections = std::shared_ptr<WellConnections>( new WellConnections( well2.getConnections()));
            connections->loadCOMPDAT(record, handlerContext.grid, name, handlerContext.keyword.location());

            if (well2.updateConnections(connections, handlerContext.grid)) {
                auto wdfac = std::make_shared<WDFAC>(well2.getWDFAC());
                wdfac->updateWDFACType(*connections);
                well2.updateWDFAC(std::move(wdfac));
                handlerContext.state().wells.update( well2 );
                wells.insert( name );
            }

            if (connections->empty() && well2.getConnections().empty()) {
                const auto& location = handlerContext.keyword.location();
                auto msg = fmt::format("Problem with COMPDAT/{}\n"
                                       "In {} line {}\n"
                                       "Well {} is not connected to grid - will remain SHUT",
                                       name, location.filename, location.lineno, name);
                OpmLog::warning(msg);
            }
            handlerContext.state().wellgroup_events().addEvent(name,
                                                               ScheduleEvents::COMPLETION_CHANGE);
        }
    }
    handlerContext.state().events().addEvent(ScheduleEvents::COMPLETION_CHANGE);

    // In the case the wells reference depth has been defaulted in the
    // WELSPECS keyword we need to force a calculation of the wells
    // reference depth exactly when the COMPDAT keyword has been completely
    // processed.
    for (const auto& wname : wells) {
        auto& well = handlerContext.state().wells.get( wname );
        well.updateRefDepth();
        handlerContext.state().wells.update( std::move(well));
    }

    if (! wells.empty()) {
        handlerContext.record_well_structure_change();
    }
}

void handleCOMPLUMP(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern);

        for (const auto& wname : well_names) {
            auto well = handlerContext.state().wells.get(wname);
            if (well.handleCOMPLUMP(record)) {
                handlerContext.state().wells.update( std::move(well) );

                handlerContext.record_well_structure_change();
            }
        }
    }
}

/*
  The COMPORD keyword is handled together with the WELSPECS keyword in the
  handleWELSPECS function.
*/
void handleCOMPORD(HandlerContext&)
{
}

void handleCOMPTRAJ(HandlerContext& handlerContext)
{
    // Keyword WELTRAJ must be read first
    std::unordered_set<std::string> wells;
    external::cvf::ref<external::cvf::BoundingBoxTree> cellSearchTree = nullptr;
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        auto wellnames = handlerContext.wellNames(wellNamePattern, false);

        for (const auto& name : wellnames) {
            auto well2 = handlerContext.state().wells.get(name);
            auto connections = std::make_shared<WellConnections>(WellConnections(well2.getConnections()));
            // cellsearchTree is calculated only once and is used to calculated cell intersections of the perforations specified in COMPTRAJ
            connections->loadCOMPTRAJ(record, handlerContext.grid, name, handlerContext.keyword.location(), cellSearchTree);
            // In the case that defaults are used in WELSPECS for headI/J the headI/J are calculated based on the well trajectory data
            well2.updateHead(connections->getHeadI(), connections->getHeadJ());
            if (well2.updateConnections(connections, handlerContext.grid)) {
                handlerContext.state().wells.update( well2 );
                wells.insert( name );
            }

            if (connections->empty() && well2.getConnections().empty()) {
                const auto& location = handlerContext.keyword.location();
                auto msg = fmt::format("Problem with COMPTRAJ/{}\n"
                                       "In {} line {}\n"
                                       "Well {} is not connected to grid - will remain SHUT", name, location.filename, location.lineno, name);
                OpmLog::warning(msg);
            }
            handlerContext.state().wellgroup_events().addEvent( name, ScheduleEvents::COMPLETION_CHANGE);
        }
    }
    handlerContext.state().events().addEvent(ScheduleEvents::COMPLETION_CHANGE);

    // In the case the wells reference depth has been defaulted in the
    // WELSPECS keyword we need to force a calculation of the wells
    // reference depth exactly when the WELCOML keyword has been completely
    // processed.
    for (const auto& wname : wells) {
        auto& well = handlerContext.state().wells.get( wname );
        well.updateRefDepth();
        handlerContext.state().wells.update( std::move(well));
    }

    if (! wells.empty()) {
        handlerContext.record_well_structure_change();
    }
}

void handleCSKIN(HandlerContext& handlerContext)
{
    // Get CSKIN keyword info and current step
    const auto& keyword = handlerContext.keyword;
    const auto& currentStep = handlerContext.currentStep;

    // Loop over records in CSKIN
    for (const auto& record: keyword) {
        // Get well names
        const auto& wellNamePattern = record.getItem( "WELL" ).getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, false);

        // Loop over well(s) in record
        for (const auto& wname : well_names) {
            // Get well information, modify connection skin factor, and update well
            auto well = handlerContext.state(currentStep).wells.get(wname);
            well.handleCSKINConnections(record);
            handlerContext.state(currentStep).wells.update( std::move(well) );
        }
    }
}

void handleWCONHIST(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, false);

        const Well::Status status = WellStatusFromString(record.getItem("STATUS").getTrimmedString(0));

        for (const auto& well_name : well_names) {
            handlerContext.updateWellStatus(well_name , status,
                                            handlerContext.keyword.location());

            std::optional<VFPProdTable::ALQ_TYPE> alq_type;
            auto well2 = handlerContext.state().wells.get( well_name );
            const bool switching_from_injector = !well2.isProducer();
            auto properties = std::make_shared<Well::WellProductionProperties>(well2.getProductionProperties());
            bool update_well = false;

            auto table_nr = record.getItem("VFP_TABLE").get< int >(0);
            if (record.getItem("VFP_TABLE").defaultApplied(0)) {
                table_nr = properties->VFPTableNumber;
            }

            if (table_nr != 0) {
                const auto& vfpprod = handlerContext.state().vfpprod;
                if (vfpprod.has(table_nr)) {
                    alq_type = handlerContext.state().vfpprod(table_nr).getALQType();
                } else {
                    std::string reason = fmt::format("Problem with well:{} VFP table: {} not defined", well_name, table_nr);
                    throw OpmInputError(reason, handlerContext.keyword.location());
                }
            }
            properties->handleWCONHIST(alq_type, handlerContext.unitSystem(), record);

            if (switching_from_injector) {
                properties->resetDefaultBHPLimit();

                auto inj_props = std::make_shared<Well::WellInjectionProperties>(well2.getInjectionProperties());
                inj_props->resetBHPLimit();
                well2.updateInjection(inj_props);
                update_well = true;
                handlerContext.state().wellgroup_events().addEvent(well2.name(),
                                                                   ScheduleEvents::WELL_SWITCHED_INJECTOR_PRODUCER);
            }

            if (well2.updateProduction(properties)) {
                update_well = true;
            }

            if (well2.updatePrediction(false)) {
                update_well = true;
            }

            if (well2.updateHasProduced()) {
                update_well = true;
            }

            if (update_well) {
                handlerContext.state().events().addEvent( ScheduleEvents::PRODUCTION_UPDATE );
                handlerContext.state().wellgroup_events().addEvent( well2.name(), ScheduleEvents::PRODUCTION_UPDATE);
                handlerContext.state().wells.update( well2 );
            }

            if (!well2.getAllowCrossFlow()) {
                // The numerical content of the rate UDAValues is accessed unconditionally;
                // since this is in history mode use of UDA values is not allowed anyway.
                const auto& oil_rate = properties->OilRate;
                const auto& water_rate = properties->WaterRate;
                const auto& gas_rate = properties->GasRate;
                if (oil_rate.zero() && water_rate.zero() && gas_rate.zero()) {
                    const auto elapsed = handlerContext.state().start_time() - handlerContext.state(0).start_time();
                    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
                    std::string msg =
                        "Well " + well2.name() + " is a history matched well with zero rate where crossflow is banned. " +
                        "This well will be closed at " + std::to_string(seconds / (60*60*24)) + " days";
                    OpmLog::note(msg);
                    handlerContext.updateWellStatus(well_name, Well::Status::SHUT);
                }
            }
        }
    }
}

void handleWCONINJE(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern);

        const Well::Status status = WellStatusFromString(record.getItem("STATUS").getTrimmedString(0));

        for (const auto& well_name : well_names) {
            handlerContext.updateWellStatus(well_name, status, handlerContext.keyword.location());

            bool update_well = false;
            auto well2 = handlerContext.state().wells.get( well_name );

            auto injection = std::make_shared<Well::WellInjectionProperties>(well2.getInjectionProperties());
            auto previousInjectorType = injection->injectorType;
            injection->handleWCONINJE(record, well2.isAvailableForGroupControl(), well_name);
            const bool switching_from_producer = well2.isProducer();
            if (well2.updateInjection(injection)) {
                update_well = true;
            }

            if (switching_from_producer) {
                handlerContext.state().wellgroup_events().addEvent(well2.name(),
                                                                   ScheduleEvents::WELL_SWITCHED_INJECTOR_PRODUCER);
            }

            if (well2.updatePrediction(true)) {
                update_well = true;
            }

            if (well2.updateHasInjected()) {
                update_well = true;
            }

            if (update_well) {
                handlerContext.state().events().addEvent(ScheduleEvents::INJECTION_UPDATE);
                handlerContext.state().wellgroup_events().addEvent(well_name,
                                                                   ScheduleEvents::INJECTION_UPDATE);
                if (previousInjectorType != injection->injectorType) {
                    handlerContext.state().wellgroup_events().addEvent(well_name,
                                                                       ScheduleEvents::INJECTION_TYPE_CHANGED);
                }
                handlerContext.state().wells.update(std::move(well2));
            }

            // if the well has zero surface rate limit or reservior rate limit, while does not allow crossflow,
            // it should be turned off.
            if ( ! well2.getAllowCrossFlow() ) {
                const auto elapsed = handlerContext.state().start_time() - handlerContext.state(0).start_time();
                const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
                std::string msg =
                    "Well " + well_name + " is an injector with zero rate where crossflow is banned. " +
                    "This well will be closed at " + std::to_string (seconds / (60*60*24) ) + " days";

                if (injection->surfaceInjectionRate.is<double>()) {
                    if (injection->hasInjectionControl(Well::InjectorCMode::RATE) && injection->surfaceInjectionRate.zero()) {
                        OpmLog::note(msg);
                        handlerContext.updateWellStatus(well_name, Well::Status::SHUT);
                    }
                }

                if (injection->reservoirInjectionRate.is<double>()) {
                    if (injection->hasInjectionControl(Well::InjectorCMode::RESV) && injection->reservoirInjectionRate.zero()) {
                        OpmLog::note(msg);
                        handlerContext.updateWellStatus(well_name, Well::Status::SHUT);
                    }
                }
            }

            if (handlerContext.state().wells.get( well_name ).getStatus() == Well::Status::OPEN) {
                handlerContext.state().wellgroup_events().addEvent(well_name,
                                                                   ScheduleEvents::REQUEST_OPEN_WELL);
            }

            auto udq_active = handlerContext.state().udq_active.get();
            const auto& udq = handlerContext.state(handlerContext.currentStep).udq.get();
            if (injection->updateUDQActive(udq, udq_active)) {
                handlerContext.state().udq_active.update(std::move(udq_active));
            }

            handlerContext.affected_well(well_name);
        }
    }
}

void handleWCONINJH(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, false);
        const Well::Status status = WellStatusFromString( record.getItem("STATUS").getTrimmedString(0));

        for (const auto& well_name : well_names) {
            handlerContext.updateWellStatus(well_name, status, handlerContext.keyword.location());
            bool update_well = false;
            auto well2 = handlerContext.state().wells.get( well_name );
            auto injection = std::make_shared<Well::WellInjectionProperties>(well2.getInjectionProperties());
            auto previousInjectorType = injection->injectorType;
            injection->handleWCONINJH(record, well2.isProducer(), well_name, handlerContext.keyword.location());
            const bool switching_from_producer = well2.isProducer();

            if (well2.updateInjection(injection)) {
                update_well = true;
            }

            if (switching_from_producer) {
                handlerContext.state().wellgroup_events().addEvent(well2.name(),
                                                                   ScheduleEvents::WELL_SWITCHED_INJECTOR_PRODUCER);
            }

            if (well2.updatePrediction(false)) {
                update_well = true;
            }

            if (well2.updateHasInjected()) {
                update_well = true;
            }

            if (update_well) {
                handlerContext.state().events().addEvent( ScheduleEvents::INJECTION_UPDATE );
                handlerContext.state().wellgroup_events().addEvent( well_name, ScheduleEvents::INJECTION_UPDATE);
                if (previousInjectorType != injection->injectorType) {
                    handlerContext.state().wellgroup_events().addEvent(well_name,
                                                                       ScheduleEvents::INJECTION_TYPE_CHANGED);
                }
                handlerContext.state().wells.update( std::move(well2) );
            }

            if ( ! well2.getAllowCrossFlow() && (injection->surfaceInjectionRate.zero())) {
                const auto elapsed = handlerContext.state().start_time() - handlerContext.state(0).start_time();
                const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
                std::string msg =
                    "Well " + well_name + " is an injector with zero rate where crossflow is banned. " +
                    "This well will be closed at " + std::to_string (seconds / (60*60*24) ) + " days";
                OpmLog::note(msg);
                handlerContext.updateWellStatus(well_name, Well::Status::SHUT);
            }
        }
    }
}

void handleWCONPROD(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, false);

        const Well::Status status = WellStatusFromString(record.getItem("STATUS").getTrimmedString(0));

        for (const auto& well_name : well_names) {
            bool update_well = handlerContext.updateWellStatus(well_name, status, handlerContext.keyword.location());
            std::optional<VFPProdTable::ALQ_TYPE> alq_type;
            auto well2 = handlerContext.state().wells.get( well_name );
            const bool switching_from_injector = !well2.isProducer();
            auto properties = std::make_shared<Well::WellProductionProperties>(well2.getProductionProperties());
            properties->clearControls();
            if (well2.isAvailableForGroupControl()) {
                properties->addProductionControl(Well::ProducerCMode::GRUP);
            }

            auto table_nr = record.getItem("VFP_TABLE").get< int >(0);
            if (record.getItem("VFP_TABLE").defaultApplied(0)) {
                table_nr = properties->VFPTableNumber;
            }

            if (table_nr != 0) {
                const auto& vfpprod = handlerContext.state().vfpprod;
                if (vfpprod.has(table_nr)) {
                    alq_type = handlerContext.state().vfpprod(table_nr).getALQType();
                } else {
                    std::string reason = fmt::format("Problem with well:{} VFP table: {} not defined", well_name, table_nr);
                    throw OpmInputError(reason, handlerContext.keyword.location());
                }
            }
            properties->handleWCONPROD(alq_type, handlerContext.unitSystem(), well_name, record);

            if (switching_from_injector) {
                properties->resetDefaultBHPLimit();
                update_well = true;
                handlerContext.state().wellgroup_events().addEvent(well2.name(),
                                                                   ScheduleEvents::WELL_SWITCHED_INJECTOR_PRODUCER);
            }

            if (well2.updateProduction(properties)) {
                update_well = true;
            }

            if (well2.updatePrediction(true)) {
                update_well = true;
            }

            if (well2.updateHasProduced()) {
                update_well = true;
            }

            if (well2.getStatus() == WellStatus::OPEN) {
                handlerContext.state().wellgroup_events().addEvent(well2.name(),
                                                                   ScheduleEvents::REQUEST_OPEN_WELL);
            }

            if (update_well) {
                handlerContext.state().events().addEvent( ScheduleEvents::PRODUCTION_UPDATE );
                handlerContext.state().wellgroup_events().addEvent( well2.name(), ScheduleEvents::PRODUCTION_UPDATE);
                handlerContext.state().wells.update( std::move(well2) );
            }

            auto udq_active = handlerContext.state().udq_active.get();
            const auto& udq = handlerContext.state(handlerContext.currentStep).udq.get();
            if (properties->updateUDQActive(udq, udq_active)) {
                handlerContext.state().udq_active.update(std::move(udq_active));
            }

            handlerContext.affected_well(well_name);
        }
    }
}

void handleWDFAC(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, true);
        if (well_names.empty()) {
            handlerContext.invalidNamePattern(wellNamePattern);
        }

        for (const auto& well_name : well_names) {
            auto well = handlerContext.state().wells.get(well_name);
            auto wdfac = std::make_shared<WDFAC>(well.getWDFAC());
            wdfac->updateWDFAC( record );
            if (well.updateWDFAC(std::move(wdfac))) {
                handlerContext.state().wells.update( std::move(well) );
            }
        }
    }
}

void handleWDFACCOR(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELLNAME").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, true);
        if (well_names.empty()) {
            handlerContext.invalidNamePattern(wellNamePattern);
        }

        for (const auto& well_name : well_names) {
            auto well = handlerContext.state().wells.get(well_name);
            auto wdfac = std::make_shared<WDFAC>(well.getWDFAC());
            wdfac->updateWDFACCOR( record );
            if (well.updateWDFAC(std::move(wdfac))) {
                handlerContext.state().wells.update( std::move(well) );
            }
        }
    }
}

void handleWECON(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, false);

        for (const auto& well_name : well_names) {
            auto well2 = handlerContext.state().wells.get( well_name );
            auto econ_limits = std::make_shared<WellEconProductionLimits>( record );
            if (well2.updateEconLimits(econ_limits)) {
                handlerContext.state().wells.update( std::move(well2) );
            }
        }
    }
}

void handleWEFAC(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELLNAME").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern);

        const double& efficiencyFactor = record.getItem("EFFICIENCY_FACTOR").get<double>(0);

        for (const auto& well_name : well_names) {
            auto well2 = handlerContext.state().wells.get( well_name );
            if (well2.updateEfficiencyFactor(efficiencyFactor)){
                handlerContext.state().wellgroup_events().addEvent( well_name, ScheduleEvents::WELLGROUP_EFFICIENCY_UPDATE);
                handlerContext.state().events().addEvent(ScheduleEvents::WELLGROUP_EFFICIENCY_UPDATE);
                handlerContext.state().wells.update( std::move(well2) );
            }
        }
    }
}

void handleWELOPEN(HandlerContext& handlerContext)
{
    const auto& keyword = handlerContext.keyword;
    const auto& currentStep = handlerContext.currentStep;

    auto conn_defaulted = []( const DeckRecord& rec ) {
        auto defaulted = []( const DeckItem& item ) {
            return item.defaultApplied( 0 );
        };

        return std::all_of( rec.begin() + 2, rec.end(), defaulted );
    };

    constexpr auto open = Well::Status::OPEN;

    for (const auto& record : keyword) {
        const auto& wellNamePattern = record.getItem( "WELL" ).getTrimmedString(0);
        const auto& status_str = record.getItem( "STATUS" ).getTrimmedString( 0 );
        const auto well_names = handlerContext.wellNames(wellNamePattern);

        /* if all records are defaulted or just the status is set, only
         * well status is updated
         */
        if (conn_defaulted(record)) {
            const auto new_well_status = WellStatusFromString(status_str);
            for (const auto& wname : well_names) {
                const auto did_update_well_status =
                    handlerContext.updateWellStatus(wname, new_well_status);

                handlerContext.affected_well(wname);

                if (did_update_well_status) {
                    handlerContext.record_well_structure_change();
                }

                if (did_update_well_status && (new_well_status == open)) {
                    // Record possible well injection/production status change
                    auto well2 = handlerContext.state(currentStep).wells.get(wname);

                    const auto did_flow_update =
                        (well2.isProducer() && well2.updateHasProduced())
                        ||
                        (well2.isInjector() && well2.updateHasInjected());

                    if (did_flow_update) {
                        handlerContext.state(currentStep).wells.update(std::move(well2));
                    }
                }

                if (new_well_status == open) {
                    handlerContext.state().wellgroup_events().addEvent(wname,
                                                                       ScheduleEvents::REQUEST_OPEN_WELL);
                }
            }
            continue;
        }

        /*
          Some of the connection information has been entered, in this case
          we *only* update the status of the connections, and not the well
          itself. Unless all connections are shut - then the well is also
          shut.
         */
        for (const auto& wname : well_names) {
            {
                auto well = handlerContext.state(currentStep).wells.get(wname);
                handlerContext.state(currentStep).wells.update(std::move(well));
            }

            const auto connection_status = Connection::StateFromString( status_str );
            {
                auto well = handlerContext.state(currentStep).wells.get(wname);
                well.handleWELOPENConnections(record, connection_status);
                handlerContext.state(currentStep).wells.update(std::move(well));
            }

            handlerContext.affected_well(wname);
            handlerContext.record_well_structure_change();

            handlerContext.state().events().addEvent(ScheduleEvents::COMPLETION_CHANGE);
        }
    }
}

void handleWELPIRuntime(HandlerContext& handlerContext)
{
    using WELL_NAME = ParserKeywords::WELPI::WELL_NAME;
    using PI        = ParserKeywords::WELPI::STEADY_STATE_PRODUCTIVITY_OR_INJECTIVITY_INDEX_VALUE;

    auto report_step = handlerContext.currentStep;
    for (const auto& record : handlerContext.keyword) {
        const auto well_names = handlerContext.wellNames(record.getItem<WELL_NAME>().getTrimmedString(0));
        const auto targetPI = record.getItem<PI>().get<double>(0);

        std::vector<bool> scalingApplicable;
        const auto& current_wellpi = *handlerContext.target_wellpi;
        for (const auto& well_name : well_names) {
            auto wellpi_iter = current_wellpi.find(well_name);
            if (wellpi_iter == current_wellpi.end())
                throw std::logic_error(fmt::format("Missing current PI for well {}", well_name));

            auto new_well = handlerContext.state(report_step).wells.get(well_name);
            auto scalingFactor = new_well.convertDeckPI(targetPI) / wellpi_iter->second;
            new_well.updateWellProductivityIndex();
            new_well.applyWellProdIndexScaling(scalingFactor, scalingApplicable);
            handlerContext.state().wells.update( std::move(new_well) );
            handlerContext.state().target_wellpi[well_name] = targetPI;

            handlerContext.affected_well(well_name);
        }
    }
}

void handleWELPI(HandlerContext& handlerContext)
{
    if (handlerContext.actionx_mode) {
        handleWELPIRuntime(handlerContext);
    } else {
        // Keyword structure
        //
        //   WELPI
        //     W1   123.45 /
        //     W2*  456.78 /
        //     *P   111.222 /
        //     **X* 333.444 /
        //   /
        //
        // Interpretation of productivity index (item 2) depends on well's preferred phase.
        using WELL_NAME = ParserKeywords::WELPI::WELL_NAME;
        using PI = ParserKeywords::WELPI::STEADY_STATE_PRODUCTIVITY_OR_INJECTIVITY_INDEX_VALUE;
        const auto& keyword = handlerContext.keyword;

        for (const auto& record : keyword) {
            const auto& wellNamePattern = record.getItem<WELL_NAME>().getTrimmedString(0);
            const auto well_names = handlerContext.wellNames(wellNamePattern);

            const auto rawProdIndex = record.getItem<PI>().get<double>(0);
            for (const auto& well_name : well_names) {
                auto well2 = handlerContext.state().wells.get(well_name);

                // Note: Need to ensure we have an independent copy of
                // well's connections because
                // Well::updateWellProductivityIndex() implicitly mutates
                // internal state in the WellConnections class.
                auto connections = std::make_shared<WellConnections>(well2.getConnections());
                well2.updateConnections(std::move(connections), true);
                if (well2.updateWellProductivityIndex()) {
                    handlerContext.state().wells.update(std::move(well2));
                }

                handlerContext.state().wellgroup_events().addEvent(well_name,
                                                                   ScheduleEvents::WELL_PRODUCTIVITY_INDEX);
                handlerContext.state().target_wellpi[well_name] = rawProdIndex;
            }
        }
        handlerContext.state().events().addEvent(ScheduleEvents::WELL_PRODUCTIVITY_INDEX);
    }
}

void handleWELSEGS(HandlerContext& handlerContext)
{
    const auto& record1 = handlerContext.keyword.getRecord(0);
    const auto& wname = record1.getItem("WELL").getTrimmedString(0);
    if (handlerContext.state(handlerContext.currentStep).wells.has(wname)) {
        auto well = handlerContext.state().wells.get(wname);
        if (well.handleWELSEGS(handlerContext.keyword)) {
            handlerContext.state().wells.update( std::move(well) );
            handlerContext.record_well_structure_change();
        }
        handlerContext.welsegs_handled(wname);
    } else {
        const auto& location = handlerContext.keyword.location();
        if (handlerContext.wgnames().has_well(wname)) {
            std::string msg = fmt::format(R"(Well: {} not yet defined for keyword {}.
Expecting well to be defined with WELSPECS in ACTIONX before actual use.
File {} line {}.)", wname, location.keyword, location.filename, location.lineno);
            OpmLog::warning(msg);
        } else
            throw OpmInputError(fmt::format("No such well: ", wname), location);
    }
}

/*
  The documentation for the WELTARG keyword says that the well
  must have been fully specified and initialized using one of the
  WCONxxxx keywords prior to modifying the well using the WELTARG
  keyword.

  The following implementation of handling the WELTARG keyword
  does not check or enforce in any way that this is done (i.e. it
  is not checked or verified that the well is initialized with any
  WCONxxxx keyword).

  Update: See the discussion following the definitions of the SI factors, due
  to a bad design we currently need the well to be specified with
  WCONPROD / WCONHIST before WELTARG is applied, if not the units for the
  rates will be wrong.
*/
void handleWELTARG(HandlerContext& handlerContext)
{
    const double SiFactorP = handlerContext.unitSystem().parse("Pressure").getSIScaling();
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern);
        if (well_names.empty()) {
            handlerContext.invalidNamePattern( wellNamePattern);
        }

        const auto cmode = WellWELTARGCModeFromString(record.getItem("CMODE").getTrimmedString(0));
        const auto new_arg = record.getItem("NEW_VALUE").get<UDAValue>(0);

        for (const auto& well_name : well_names) {
            auto well2 = handlerContext.state().wells.get(well_name);
            bool update = false;
            if (well2.isProducer()) {
                auto prop = std::make_shared<Well::WellProductionProperties>(well2.getProductionProperties());
                prop->handleWELTARG(cmode, new_arg, SiFactorP);
                update = well2.updateProduction(prop);
                if (cmode == Well::WELTARGCMode::GUID) {
                    update |= well2.updateWellGuideRate(new_arg.get<double>());
                }

                auto udq_active = handlerContext.state().udq_active.get();
                const auto& udq = handlerContext.state(handlerContext.currentStep).udq.get();
                if (prop->updateUDQActive(udq, cmode, udq_active)) {
                    handlerContext.state().udq_active.update( std::move(udq_active));
                }
            }
            else {
                auto inj = std::make_shared<Well::WellInjectionProperties>(well2.getInjectionProperties());
                inj->handleWELTARG(cmode, new_arg, SiFactorP);
                update = well2.updateInjection(inj);
                if (cmode == Well::WELTARGCMode::GUID) {
                    update |= well2.updateWellGuideRate(new_arg.get<double>());
                }

                auto udq_active = handlerContext.state().udq_active.get();
                const auto& udq = handlerContext.state(handlerContext.currentStep).udq.get();
                if (inj->updateUDQActive(udq, cmode, udq_active)) {
                    handlerContext.state().udq_active.update(std::move(udq_active));
                }
            }

            if (update)
            {
                if (well2.isProducer()) {
                    handlerContext.state().wellgroup_events().addEvent( well_name, ScheduleEvents::PRODUCTION_UPDATE);
                    handlerContext.state().events().addEvent( ScheduleEvents::PRODUCTION_UPDATE );
                } else {
                    handlerContext.state().wellgroup_events().addEvent( well_name, ScheduleEvents::INJECTION_UPDATE);
                    handlerContext.state().events().addEvent( ScheduleEvents::INJECTION_UPDATE );
                }
                handlerContext.state().wells.update( std::move(well2) );
            }

            handlerContext.affected_well(well_name);
        }
    }
}

void handleWELTRAJ(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        auto wellnames = handlerContext.wellNames(wellNamePattern, false);

        for (const auto& name : wellnames) {
            auto well2 = handlerContext.state().wells.get(name);
            auto connections = std::make_shared<WellConnections>(WellConnections(well2.getConnections()));
            connections->loadWELTRAJ(record, handlerContext.grid, name, handlerContext.keyword.location());
            if (well2.updateConnections(connections, handlerContext.grid)) {
                handlerContext.state().wells.update( well2 );
                handlerContext.record_well_structure_change();
            }
            handlerContext.state().wellgroup_events().addEvent( name, ScheduleEvents::COMPLETION_CHANGE);
            const auto& md = connections->getMD();
            if (!std::is_sorted(std::begin(md), std::end(md))) {
                auto msg = fmt::format("Well {} measured depth column is not strictly increasing", name);
                throw OpmInputError(msg, handlerContext.keyword.location());
            }
        }
    }
    handlerContext.state().events().addEvent(ScheduleEvents::COMPLETION_CHANGE);
}

void handleWFOAM(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, false);

        for (const auto& well_name : well_names) {
            auto well2 = handlerContext.state().wells.get(well_name);
            auto foam_properties = std::make_shared<WellFoamProperties>(well2.getFoamProperties());
            foam_properties->handleWFOAM(record);
            if (well2.updateFoamProperties(foam_properties)) {
                handlerContext.state().wells.update( std::move(well2) );
            }
        }
    }
}

void handleWGRUPCON(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern);

        const bool availableForGroupControl = DeckItem::to_bool(record.getItem("GROUP_CONTROLLED").getTrimmedString(0));
        const double guide_rate = record.getItem("GUIDE_RATE").get<double>(0);
        const double scaling_factor = record.getItem("SCALING_FACTOR").get<double>(0);

        for (const auto& well_name : well_names) {
            auto phase = Well::GuideRateTarget::UNDEFINED;
            if (!record.getItem("PHASE").defaultApplied(0)) {
                std::string guideRatePhase = record.getItem("PHASE").getTrimmedString(0);
                phase = WellGuideRateTargetFromString(guideRatePhase);
            }

            auto well = handlerContext.state().wells.get(well_name);
            if (well.updateWellGuideRate(availableForGroupControl, guide_rate, phase,
                                         scaling_factor)) {
                auto new_config = handlerContext.state().guide_rate();
                new_config.update_well(well);
                handlerContext.state().guide_rate.update( std::move(new_config) );
                handlerContext.state().wells.update( std::move(well) );
            }
        }
    }
}

void handleWHISTCTL(HandlerContext& handlerContext)
{
    const auto& record = handlerContext.keyword.getRecord(0);
    const std::string& cmodeString = record.getItem("CMODE").getTrimmedString(0);
    const auto controlMode = WellProducerCModeFromString(cmodeString);

    if (controlMode != Well::ProducerCMode::NONE) {
        if (!Well::WellProductionProperties::effectiveHistoryProductionControl(controlMode) ) {
            std::string msg = "The WHISTCTL keyword specifies an un-supported control mode " + cmodeString
                + ", which makes WHISTCTL keyword not affect the simulation at all";
            OpmLog::warning(msg);
        } else
            handlerContext.state().update_whistctl( controlMode );
    }

    const std::string bhp_terminate = record.getItem("BPH_TERMINATE").getTrimmedString(0);
    if (bhp_terminate == "YES") {
        std::string msg_fmt = "Problem with {keyword}\n"
                              "In {file} line {line}\n"
                              "Setting item 2 in {keyword} to 'YES' to stop the run is not supported";
        handlerContext.parseContext.handleError( ParseContext::UNSUPPORTED_TERMINATE_IF_BHP , msg_fmt, handlerContext.keyword.location(), handlerContext.errors );
    }

    for (const auto& well_ref : handlerContext.state().wells()) {
        auto well2 = well_ref.get();
        auto prop = std::make_shared<Well::WellProductionProperties>(well2.getProductionProperties());

        if (prop->whistctl_cmode != controlMode) {
            prop->whistctl_cmode = controlMode;
            well2.updateProduction(prop);
            handlerContext.state().wells.update( std::move(well2) );
        }
    }
}

void handleWINJCLN(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem<ParserKeywords::WINJCLN::WELL_NAME>().getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, false);
        for (const auto& well_name : well_names) {
            auto well = handlerContext.state().wells(well_name);
            well.handleWINJCLN(record, handlerContext.keyword.location());
            handlerContext.state().wells.update(std::move(well));
        }
    }
}

void handleWINJDAM(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem<ParserKeywords::WINJDAM::WELL_NAME>().getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, false);

        for (const auto& well_name : well_names) {
            auto well = handlerContext.state().wells(well_name);
            if (well.handleWINJDAM(record, handlerContext.keyword.location())) {
                handlerContext.state().wells.update( std::move(well) );
            }
        }
    }
}

void handleWINJFCNC(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem<ParserKeywords::WINJFCNC::WELL>().getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, false);
        for (const auto& well_name : well_names) {
            auto well = handlerContext.state().wells(well_name);
            const auto filter_conc = record.getItem<ParserKeywords::WINJFCNC::VOL_CONCENTRATION>().get<UDAValue>(0);
            well.setFilterConc(filter_conc );
            handlerContext.state().wells.update(std::move(well));
        }
    }
}

void handleWINJMULT(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL_NAME").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, false);

        for (const auto& well_name : well_names) {
            auto well = handlerContext.state().wells( well_name );
            if (well.isProducer()) {
                const std::string reason = fmt::format("Keyword WINJMULT can only apply to injectors,"
                                                       " but Well {} is a producer", well_name);
                throw OpmInputError(reason, handlerContext.keyword.location());
            }
            if (well.handleWINJMULT(record, handlerContext.keyword.location())) {
                handlerContext.state().wells.update(std::move(well));
            }
        }
    }
}

void handleWINJTEMP(HandlerContext& handlerContext)
{
    // we do not support the "enthalpy" field yet. how to do this is a more difficult
    // question.
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        auto well_names = handlerContext.wellNames(wellNamePattern, false);

        const double temp = record.getItem("TEMPERATURE").getSIDouble(0);

        for (const auto& well_name : well_names) {
            const auto& well = handlerContext.state(handlerContext.currentStep).wells.get(well_name);
            const double current_temp = !well.isProducer()? well.temperature(): 0.0;
            if (current_temp != temp) {
                auto well2 = handlerContext.state().wells( well_name );
                well2.setWellTemperature(temp);
                handlerContext.state().wells.update( std::move(well2) );
            }
        }
    }
}

void handleWLIFTOPT(HandlerContext& handlerContext)
{
    auto glo = handlerContext.state().glo();

    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem<ParserKeywords::WLIFTOPT::WELL>().getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, true);
        if (well_names.empty()) {
            handlerContext.invalidNamePattern(wellNamePattern);
        }

        const bool use_glo = DeckItem::to_bool(record.getItem<ParserKeywords::WLIFTOPT::USE_OPTIMIZER>().get<std::string>(0));
        const bool alloc_extra_gas = DeckItem::to_bool( record.getItem<ParserKeywords::WLIFTOPT::ALLOCATE_EXTRA_LIFT_GAS>().get<std::string>(0));
        const double weight_factor = record.getItem<ParserKeywords::WLIFTOPT::WEIGHT_FACTOR>().get<double>(0);
        const double inc_weight_factor = record.getItem<ParserKeywords::WLIFTOPT::DELTA_GAS_RATE_WEIGHT_FACTOR>().get<double>(0);
        const double min_rate = record.getItem<ParserKeywords::WLIFTOPT::MIN_LIFT_GAS_RATE>().getSIDouble(0);
        const auto& max_rate_item = record.getItem<ParserKeywords::WLIFTOPT::MAX_LIFT_GAS_RATE>();

        for (const auto& wname : well_names) {
            auto well = GasLiftWell(wname, use_glo);

            if (max_rate_item.hasValue(0)) {
                well.max_rate( max_rate_item.getSIDouble(0) );
            }

            well.weight_factor(weight_factor);
            well.inc_weight_factor(inc_weight_factor);
            well.min_rate(min_rate);
            well.alloc_extra_gas(alloc_extra_gas);

            glo.add_well(well);
        }
    }

    handlerContext.state().glo.update( std::move(glo) );
}

void handleWLIST(HandlerContext& handlerContext)
{
    const std::string legal_actions = "NEW:ADD:DEL:MOV";
    for (const auto& record : handlerContext.keyword) {
        const std::string& name = record.getItem("NAME").getTrimmedString(0);
        const std::string& action = record.getItem("ACTION").getTrimmedString(0);
        const std::vector<std::string>& well_args = record.getItem("WELLS").getData<std::string>();
        std::vector<std::string> wells;
        auto new_wlm = handlerContext.state().wlist_manager.get();

        if (legal_actions.find(action) == std::string::npos) {
            throw std::invalid_argument("The action:" + action + " is not recognized.");
        }

        for (const auto& well_arg : well_args) {
            const auto& names = handlerContext.wellNames(well_arg, true);
            if (names.empty() && well_arg.find("*") == std::string::npos) {
                throw std::invalid_argument("The well: " + well_arg + " has not been defined in the WELSPECS");
            }

            std::move(names.begin(), names.end(), std::back_inserter(wells));
        }

        if (name[0] != '*') {
            throw std::invalid_argument("The list name in WLIST must start with a '*'");
        }

        if (action == "NEW") {
            new_wlm.newList(name, wells);
        }

        if (!new_wlm.hasList(name)) {
            throw std::invalid_argument("Invalid well list: " + name);
        }

        if (action == "MOV") {
            for (const auto& well : wells) {
                new_wlm.delWell(well);
            }
        }

        if (action == "DEL") {
            for (const auto& well : wells) {
                new_wlm.delWListWell(well, name);
            }
        } else if (action != "NEW"){
            for (const auto& well : wells) {
                new_wlm.addWListWell(well, name);
            }
        }
        handlerContext.state().wlist_manager.update( std::move(new_wlm) );
    }
}

void handleWMICP(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, false);

        for (const auto& well_name : well_names) {
            auto well = handlerContext.state().wells( well_name );
            auto micp_properties = std::make_shared<WellMICPProperties>( well.getMICPProperties() );
            micp_properties->handleWMICP(record);
            if (well.updateMICPProperties(micp_properties)) {
                handlerContext.state().wells.update( std::move(well));
            }
        }
    }
}

void handleWPAVE(HandlerContext& handlerContext)
{
    auto wpave = PAvg(handlerContext.keyword.getRecord(0));

    if (wpave.inner_weight() > 1.0) {
        const auto reason =
            fmt::format("Inner block weighting F1 "
                        "must not exceed 1.0. Got {}",
                        wpave.inner_weight());

        throw OpmInputError {
            reason, handlerContext.keyword.location()
        };
    }

    if ((wpave.conn_weight() < 0.0) ||
        (wpave.conn_weight() > 1.0))
    {
        const auto reason =
            fmt::format("Connection weighting factor F2 "
                        "must be between zero and one "
                        "inclusive. Got {} instead.",
                        wpave.conn_weight());

        throw OpmInputError {
            reason, handlerContext.keyword.location()
        };
    }

    for (const auto& wname : handlerContext.state(handlerContext.currentStep).well_order().names()) {
        const auto& well = handlerContext.state().wells.get(wname);
        if (well.pavg() != wpave) {
            auto new_well = well;
            new_well.updateWPAVE( wpave );
            handlerContext.state().wells.update( std::move(new_well) );
        }
    }

    handlerContext.state().pavg.update(std::move(wpave));
}

void handleWPAVEDEP(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem<ParserKeywords::WPAVEDEP::WELL>().getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, true);

        if (well_names.empty()) {
            handlerContext.invalidNamePattern(wellNamePattern);
        }

        const auto& item = record.getItem<ParserKeywords::WPAVEDEP::REFDEPTH>();
        if (item.hasValue(0)) {
            auto ref_depth = item.getSIDouble(0);
            for (const auto& well_name : well_names) {
                auto well = handlerContext.state().wells.get(well_name);
                well.updateWPaveRefDepth( ref_depth );
                handlerContext.state().wells.update( std::move(well) );
            }
        }
    }
}

void handleWPIMULT(HandlerContext& handlerContext)
{
    // from the third item to the seventh item in the WPIMULT record, they are numbers indicate
    // the I, J, K location and completion number range.
    // When defaulted, it assumes it is negative
    // When inputting a negative value, it assumes it is defaulted.
    auto defaultConCompRec = [](const DeckRecord& wpimult)
    {
        return std::all_of(wpimult.begin() + 2, wpimult.end(),
            [](const DeckItem& item)
            {
                return item.defaultApplied(0) || (item.get<int>(0) < 0);
            });
    };

    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto& well_names = handlerContext.wellNames(wellNamePattern);

        // for the record has defaulted connection and completion information, we do not apply it immediately
        // because we only need to apply the last record with defaulted connection and completion information
        // as a result, we here only record the information of the record with defaulted connection and completion
        // information without applying, because there might be multiple WPIMULT keywords here, and we do not know
        // whether it is the last one.
        const bool default_con_comp = defaultConCompRec(record);
        if (default_con_comp) {
            auto wpimult_global_factor = handlerContext.wpimult_global_factor;
            if (!wpimult_global_factor) {
                throw std::runtime_error(" wpimult_global_factor is nullptr in function handleWPIMULT ");
            }
            const auto scaling_factor = record.getItem("WELLPI").get<double>(0);
            for (const auto& wname : well_names) {
                (*wpimult_global_factor)[wname] = scaling_factor;
            }
            continue;
        }

        // the record with non-defaulted connection and completion information will be applied immediately
        for (const auto& wname : well_names) {
            auto well = handlerContext.state().wells( wname );
            if (well.handleWPIMULT(record)) {
                handlerContext.state().wells.update( std::move(well));
            }
        }
    }
}

void handleWPMITAB(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, false);

        for (const auto& well_name : well_names) {
            auto well = handlerContext.state().wells( well_name );
            auto polymer_properties = std::make_shared<WellPolymerProperties>( well.getPolymerProperties() );
            polymer_properties->handleWPMITAB(record);
            if (well.updatePolymerProperties(polymer_properties)) {
                handlerContext.state().wells.update( std::move(well));
            }
        }
    }
}

void handleWPOLYMER(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, false);

        for (const auto& well_name : well_names) {
            auto well = handlerContext.state().wells( well_name );
            auto polymer_properties = std::make_shared<WellPolymerProperties>( well.getPolymerProperties() );
            polymer_properties->handleWPOLYMER(record);
            if (well.updatePolymerProperties(polymer_properties)) {
                handlerContext.state().wells.update( std::move(well));
            }
        }
    }
}

void handleWRFT(HandlerContext& handlerContext)
{
    auto new_rft = handlerContext.state().rft_config();

    for (const auto& record : handlerContext.keyword) {
        const auto& item = record.getItem<ParserKeywords::WRFT::WELL>();
        if (! item.hasValue(0)) {
            continue;
        }

        const auto wellNamePattern = record.getItem<ParserKeywords::WRFT::WELL>().getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, true);

        if (well_names.empty()) {
            handlerContext.invalidNamePattern(wellNamePattern);
        }

        for (const auto& well_name : well_names) {
            new_rft.update(well_name, RFTConfig::RFT::YES);
        }
    }

    new_rft.first_open(true);

    handlerContext.state().rft_config.update(std::move(new_rft));
}

void handleWRFTPLT(HandlerContext& handlerContext)
{
    auto new_rft = handlerContext.state().rft_config();

    const auto rftKey = [](const DeckItem& key)
    {
        return RFTConfig::RFTFromString(key.getTrimmedString(0));
    };

    const auto pltKey = [](const DeckItem& key)
    {
        return RFTConfig::PLTFromString(key.getTrimmedString(0));
    };

    for (const auto& record : handlerContext.keyword) {
        const auto wellNamePattern = record.getItem<ParserKeywords::WRFTPLT::WELL>().getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, true);

        if (well_names.empty()) {
            handlerContext.invalidNamePattern(wellNamePattern);
            continue;
        }

        const auto RFTKey = rftKey(record.getItem<ParserKeywords::WRFTPLT::OUTPUT_RFT>());
        const auto PLTKey = pltKey(record.getItem<ParserKeywords::WRFTPLT::OUTPUT_PLT>());
        const auto SEGKey = pltKey(record.getItem<ParserKeywords::WRFTPLT::OUTPUT_SEGMENT>());

        for (const auto& well_name : well_names) {
            new_rft.update(well_name, RFTKey);
            new_rft.update(well_name, PLTKey);
            new_rft.update_segment(well_name, SEGKey);
        }
    }

    handlerContext.state().rft_config.update(std::move(new_rft));
}

void handleWSALT(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, false);

        for (const auto& well_name : well_names) {
            auto well2 = handlerContext.state().wells( well_name );
            auto brine_properties = std::make_shared<WellBrineProperties>(well2.getBrineProperties());
            brine_properties->handleWSALT(record);
            if (well2.updateBrineProperties(brine_properties)) {
                handlerContext.state().wells.update( std::move(well2) );
            }
        }
    }
}

void handleWSKPTAB(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, false);

        for (const auto& well_name : well_names) {
            auto well = handlerContext.state().wells( well_name );

            auto polymer_properties = std::make_shared<WellPolymerProperties>(well.getPolymerProperties());
            polymer_properties->handleWSKPTAB(record);
            if (well.updatePolymerProperties(polymer_properties)) {
                handlerContext.state().wells.update( std::move(well) );
            }
        }
    }
}

void handleWSOLVENT(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern , false);

        const double fraction = record.getItem("SOLVENT_FRACTION").get<UDAValue>(0).getSI();

        for (const auto& well_name : well_names) {
            const auto& well = handlerContext.state(handlerContext.currentStep).wells.get(well_name);
            const auto& inj = well.getInjectionProperties();
            if (!well.isProducer() && inj.injectorType == InjectorType::GAS) {
                if (well.getSolventFraction() != fraction) {
                    auto well2 = handlerContext.state().wells( well_name );
                    well2.updateSolventFraction(fraction);
                    handlerContext.state().wells.update( std::move(well2) );
                }
            } else {
                throw std::invalid_argument("The WSOLVENT keyword can only be applied to gas injectors");
            }
        }
    }
}

void handleWTEMP(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames( wellNamePattern, false);
        double temp = record.getItem("TEMP").getSIDouble(0);

        for (const auto& well_name : well_names) {
            const auto& well = handlerContext.state(handlerContext.currentStep).wells.get(well_name);
            const double current_temp = !well.isProducer()? well.temperature(): 0.0;
            if (current_temp != temp) {
                auto well2 = handlerContext.state().wells( well_name );
                well2.setWellTemperature(temp);
                handlerContext.state().wells.update( std::move(well2) );
            }
        }
    }
}

void handleWTEST(HandlerContext& handlerContext)
{
    auto new_config = handlerContext.state().wtest_config.get();
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern);
        if (well_names.empty()) {
            handlerContext.invalidNamePattern(wellNamePattern);
        }

        const double test_interval = record.getItem("INTERVAL").getSIDouble(0);
        const std::string& reasons = record.getItem("REASON").get<std::string>(0);
        const int num_test = record.getItem("TEST_NUM").get<int>(0);
        const double startup_time = record.getItem("START_TIME").getSIDouble(0);

        for (const auto& well_name : well_names) {
            if (reasons.empty())
                new_config.drop_well(well_name);
            else
                new_config.add_well(well_name, reasons, test_interval, num_test, startup_time, handlerContext.currentStep);
        }
    }
    handlerContext.state().wtest_config.update( std::move(new_config) );
}

/*
  The WTMULT keyword can optionally use UDA values in three different ways:

    1. The target can be UDA - instead of the standard strings "ORAT", "GRAT",
       "WRAT", ..., the keyword can be configured with a UDA which is evaluated to
       an integer and then mapped to one of the common controls.

    2. The scaling factor itself can be a UDA.

    3. The target we aim to scale might already be specified as a UDA.

  The current implementation does not support UDA usage in any part of WTMULT
  codepath.
*/

void handleWTMULT(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const auto& wellNamePattern = record.getItem<ParserKeywords::WTMULT::WELL>().getTrimmedString(0);
        const auto& control = record.getItem<ParserKeywords::WTMULT::CONTROL>().get<std::string>(0);
        const auto& factor = record.getItem<ParserKeywords::WTMULT::FACTOR>().get<UDAValue>(0);
        const auto& num = record.getItem<ParserKeywords::WTMULT::NUM>().get<int>(0);

        if (factor.is<std::string>()) {
            std::string reason = fmt::format("Use of UDA value: {} is not supported as multiplier", factor.get<std::string>());
            throw OpmInputError(reason, handlerContext.keyword.location());
        }

        if (handlerContext.state().udq().has_keyword(control)) {
            std::string reason = fmt::format("Use of UDA value: {} is not supported for control target", control);
            throw OpmInputError(reason, handlerContext.keyword.location());
        }

        if (num != 1) {
            std::string reason = fmt::format("Only NUM=1 is supported in WTMULT keyword");
            throw OpmInputError(reason, handlerContext.keyword.location());
        }

        const auto cmode = WellWELTARGCModeFromString(control);
        if (cmode == Well::WELTARGCMode::GUID)
            throw std::logic_error("Multiplying guide rate is not implemented");

        const auto well_names = handlerContext.wellNames(wellNamePattern);
        for (const auto& well_name : well_names) {
            auto well = handlerContext.state().wells.get(well_name);
            if (well.isInjector()) {
                bool update_well = true;
                auto properties = std::make_shared<Well::WellInjectionProperties>(well.getInjectionProperties());
                properties->handleWTMULT( cmode, factor.get<double>());

                well.updateInjection(properties);
                if (update_well) {
                    handlerContext.state().events().addEvent(ScheduleEvents::INJECTION_UPDATE);
                    handlerContext.state().wellgroup_events().addEvent(well_name, ScheduleEvents::INJECTION_UPDATE);
                    handlerContext.state().wells.update(std::move(well));
                }
            } else {
                bool update_well = true;
                auto properties = std::make_shared<Well::WellProductionProperties>(well.getProductionProperties());
                properties->handleWTMULT( cmode, factor.get<double>());

                well.updateProduction(properties);
                if (update_well) {
                    handlerContext.state().events().addEvent(ScheduleEvents::PRODUCTION_UPDATE);
                    handlerContext.state().wellgroup_events().addEvent(well_name,
                                                                       ScheduleEvents::PRODUCTION_UPDATE);
                    handlerContext.state().wells.update(std::move(well));
                }
            }
        }
    }
}

void handleWTRACER(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, true);

        if (well_names.empty()) {
            handlerContext.invalidNamePattern(wellNamePattern);
        }

        const double tracerConcentration = record.getItem("CONCENTRATION").get<UDAValue>(0).getSI();
        const std::string& tracerName = record.getItem("TRACER").getTrimmedString(0);

        for (const auto& well_name : well_names) {
            auto well = handlerContext.state().wells.get( well_name );
            auto wellTracerProperties = std::make_shared<WellTracerProperties>(well.getTracerProperties());
            wellTracerProperties->setConcentration(tracerName, tracerConcentration);
            if (well.updateTracer(wellTracerProperties)) {
                handlerContext.state().wells.update(std::move(well));
            }
        }
    }
}

void handleWVFPDP(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, true);
        if (well_names.empty()) {
            handlerContext.invalidNamePattern(wellNamePattern);
        }

        for (const auto& well_name : well_names) {
            auto well = handlerContext.state().wells.get(well_name);
            auto wvfpdp = std::make_shared<WVFPDP>(well.getWVFPDP());
            wvfpdp->update( record );
            if (well.updateWVFPDP(std::move(wvfpdp))) {
                handlerContext.state().wells.update( std::move(well) );
            }
        }
    }
}

void handleWVFPEXP(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, true);
        if (well_names.empty()) {
            handlerContext.invalidNamePattern(wellNamePattern);
        }

        for (const auto& well_name : well_names) {
            auto well = handlerContext.state().wells.get(well_name);
            auto wvfpexp = std::make_shared<WVFPEXP>(well.getWVFPEXP());
            wvfpexp->update( record );
            if (well.updateWVFPEXP(std::move(wvfpexp))) {
                handlerContext.state().wells.update( std::move(well) );
            }
        }
    }
}

void handleWWPAVE(HandlerContext& handlerContext)
{
    for (const auto& record : handlerContext.keyword) {
        const std::string& wellNamePattern = record.getItem("WELL").getTrimmedString(0);
        const auto well_names = handlerContext.wellNames(wellNamePattern, true);
        if (well_names.empty()) {
            handlerContext.invalidNamePattern(wellNamePattern);
        }

        auto wpave = PAvg(record);

        if (wpave.inner_weight() > 1.0) {
            const auto reason =
                fmt::format("Inner block weighting F1 "
                            "must not exceed one. Got {}",
                            wpave.inner_weight());

            throw OpmInputError {
                reason, handlerContext.keyword.location()
            };
        }

        if ((wpave.conn_weight() < 0.0) ||
            (wpave.conn_weight() > 1.0))
        {
            const auto reason =
                fmt::format("Connection weighting factor F2 "
                            "must be between zero and one, "
                            "inclusive. Got {} instead.",
                            wpave.conn_weight());

            throw OpmInputError {
                reason, handlerContext.keyword.location()
            };
        }

        for (const auto& well_name : well_names) {
          const auto& well = handlerContext.state().wells.get(well_name);
          if (well.pavg() != wpave) {
              auto new_well = well;
              new_well.updateWPAVE( wpave );
              handlerContext.state().wells.update( std::move(new_well) );
          }
        }
    }
}

void handleWELSPECS(HandlerContext& handlerContext)
{
    using Kw = ParserKeywords::WELSPECS;

    auto getTrimmedName = [&handlerContext](const auto& item)
    {
        return trim_wgname(handlerContext.keyword,
                           item.template get<std::string>(0),
                           handlerContext.parseContext,
                           handlerContext.errors);
    };

    auto fieldWells = std::vector<std::string>{};
    for (const auto& record : handlerContext.keyword) {
        if (const auto fip_region_number = record.getItem<Kw::FIP_REGION>().get<int>(0);
            fip_region_number != Kw::FIP_REGION::defaultValue)
        {
            const auto& location = handlerContext.keyword.location();
            const auto msg = fmt::format("Non-defaulted FIP region {} in WELSPECS keyword "
                                         "in file {} line {} is not supported. "
                                         "Reset to default value {}.",
                                         fip_region_number,
                                         location.filename,
                                         location.lineno,
                                         Kw::FIP_REGION::defaultValue);
            OpmLog::warning(msg);
        }

        if (const auto& density_calc_type = record.getItem<Kw::DENSITY_CALC>().get<std::string>(0);
            density_calc_type != Kw::DENSITY_CALC::defaultValue)
        {
            const auto& location = handlerContext.keyword.location();
            const auto msg = fmt::format("Non-defaulted density calculation method '{}' "
                                         "in WELSPECS keyword in file {} line {} is "
                                         "not supported. Reset to default value {}.",
                                         density_calc_type,
                                         location.filename,
                                         location.lineno,
                                         Kw::DENSITY_CALC::defaultValue);
            OpmLog::warning(msg);
        }

        const auto wellName = getTrimmedName(record.getItem<Kw::WELL>());
        const auto groupName = getTrimmedName(record.getItem<Kw::GROUP>());

        // We might get here from an ACTIONX context, or we might get
        // called on a well (list) template, to reassign certain well
        // properties--e.g, the well's controlling group--so check if
        // 'wellName' matches any existing well names through pattern
        // matching before treating the wellName as a simple well name.
        //
        // An empty list of well names is okay since that means we're
        // creating a new well in this case.
        constexpr auto allowEmptyWellList = true;
        const auto existingWells =
            handlerContext.wellNames(wellName, allowEmptyWellList);

        if (groupName == "FIELD") {
            if (existingWells.empty()) {
                fieldWells.push_back(wellName);
            }
            else {
                for (const auto& existingWell : existingWells) {
                    fieldWells.push_back(existingWell);
                }
            }
        }

        if (! handlerContext.state().groups.has(groupName)) {
            handlerContext.addGroup(groupName);
        }

        if (existingWells.empty()) {
            // 'wellName' does not match any existing wells.  Create a
            // new Well object for this well.
            handlerContext.welspecsCreateNewWell(record,
                                                 wellName,
                                                 groupName);
        }
        else {
            // 'wellName' matches one or more existing wells.  Assign
            // new properties for those wells.
            handlerContext.welspecsUpdateExistingWells(record,
                                                       existingWells,
                                                       groupName);
        }
    }

    if (! fieldWells.empty()) {
        std::sort(fieldWells.begin(), fieldWells.end());
        fieldWells.erase(std::unique(fieldWells.begin(), fieldWells.end()),
                         fieldWells.end());

        const auto* plural = (fieldWells.size() == 1) ? "" : "s";

        const auto msg_fmt = fmt::format(R"(Well{0} parented directly to 'FIELD'; this is allowed but discouraged.
Well{0} entered with 'FIELD' parent group:
* {1})", plural, fmt::join(fieldWells, "\n * "));

        handlerContext.parseContext.handleError(ParseContext::SCHEDULE_WELL_IN_FIELD_GROUP,
                                                msg_fmt,
                                                handlerContext.keyword.location(),
                                                handlerContext.errors);
    }

    if (! handlerContext.keyword.empty()) {
        handlerContext.record_well_structure_change();
    }
}

}

bool handleWellKeyword(HandlerContext& handlerContext)
{
    using handler_function = std::function<void(HandlerContext&)>;
    static const std::unordered_map<std::string, handler_function> handler_functions = {
        { "COMPDAT" , &handleCOMPDAT   },
        { "COMPLUMP", &handleCOMPLUMP  },
        { "COMPORD" , &handleCOMPORD   },
        { "COMPTRAJ", &handleCOMPTRAJ  },
        { "CSKIN"   , &handleCSKIN     },
        { "WCONHIST", &handleWCONHIST  },
        { "WCONINJE", &handleWCONINJE  },
        { "WCONINJH", &handleWCONINJH  },
        { "WCONPROD", &handleWCONPROD  },
        { "WDFAC"   , &handleWDFAC     },
        { "WDFACCOR", &handleWDFACCOR  },
        { "WECON"   , &handleWECON     },
        { "WEFAC"   , &handleWEFAC     },
        { "WELOPEN" , &handleWELOPEN   },
        { "WELPI"   , &handleWELPI     },
        { "WELSEGS" , &handleWELSEGS   },
        { "WELSPECS", &handleWELSPECS  },
        { "WELTARG" , &handleWELTARG   },
        { "WELTRAJ" , &handleWELTRAJ   },
        { "WFOAM"   , &handleWFOAM     },
        { "WGRUPCON", &handleWGRUPCON  },
        { "WHISTCTL", &handleWHISTCTL  },
        { "WINJCLN" , &handleWINJCLN   },
        { "WINJDAM" , &handleWINJDAM   },
        { "WINJFCNC", &handleWINJFCNC  },
        { "WINJMULT", &handleWINJMULT  },
        { "WINJTEMP", &handleWINJTEMP  },
        { "WLIFTOPT", &handleWLIFTOPT  },
        { "WLIST"   , &handleWLIST     },
        { "WMICP"   , &handleWMICP     },
        { "WPAVE"   , &handleWPAVE     },
        { "WPAVEDEP", &handleWPAVEDEP  },
        { "WPIMULT" , &handleWPIMULT   },
        { "WPMITAB" , &handleWPMITAB   },
        { "WPOLYMER", &handleWPOLYMER  },
        { "WRFT"    , &handleWRFT      },
        { "WRFTPLT" , &handleWRFTPLT   },
        { "WSALT"   , &handleWSALT     },
        { "WSKPTAB" , &handleWSKPTAB   },
        { "WSOLVENT", &handleWSOLVENT  },
        { "WTEMP"   , &handleWTEMP     },
        { "WTEST"   , &handleWTEST     },
        { "WTMULT"  , &handleWTMULT    },
        { "WTRACER" , &handleWTRACER   },
        { "WVFPDP"  , &handleWVFPDP    },
        { "WVFPEXP" , &handleWVFPEXP   },
        { "WWPAVE"  , &handleWWPAVE    },
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
