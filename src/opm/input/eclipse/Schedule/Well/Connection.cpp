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

#include <opm/input/eclipse/Schedule/Well/Connection.hpp>

#include <opm/io/eclipse/rst/connection.hpp>

#include <opm/input/eclipse/Deck/DeckItem.hpp>
#include <opm/input/eclipse/Deck/DeckKeyword.hpp>
#include <opm/input/eclipse/Deck/DeckRecord.hpp>

#include <opm/input/eclipse/EclipseState/Grid/FieldPropsManager.hpp>

#include <opm/input/eclipse/Schedule/ScheduleGrid.hpp>
#include <opm/input/eclipse/Schedule/Well/FilterCake.hpp>

#include <cassert>
#include <sstream>
#include <stdexcept>

namespace {
    constexpr bool restartDefaultSatTabId = true;
}

namespace Opm {

    Connection::CTFProperties
    Connection::CTFProperties::serializationTestObject()
    {
        auto props = Opm::Connection::CTFProperties{};

        props.CF = 1.0;
        props.Kh = 2.0;
        props.Ke = 3.0;
        props.rw = 4.0;
        props.r0 = 5.0;
        props.re = 6.0;
        props.connection_length = 7.0;
        props.skin_factor = 8.0;
        props.d_factor = 9.0;
        props.peaceman_denom = 10.0;

        return props;
    }

    bool Connection::CTFProperties::operator==(const CTFProperties& that) const
    {
        return (this->CF == that.CF)
            && (this->Kh == that.Kh)
            && (this->Ke == that.Ke)
            && (this->rw == that.rw)
            && (this->r0 == that.r0)
            && (this->re == that.re)
            && (this->connection_length == that.connection_length)
            && (this->skin_factor == that.skin_factor)
            && (this->d_factor == that.d_factor)
            && (this->peaceman_denom == that.peaceman_denom)
            ;
    }

    // =========================================================================

    Connection::Connection(const int i, const int j, const int k,
                           const std::size_t    global_index,
                           const int            complnum,
                           const double         depth,
                           const State          stateArg,
                           const double         CF,
                           const double         Kh,
                           const double         rw,
                           const double         r0,
                           const double         re,
                           const double         connection_length,
                           const double         skin_factor,
                           const double         d_factor,
                           const double         Ke,
                           const int            satTableId,
                           const Direction      directionArg,
                           const CTFKind        ctf_kind,
                           const std::size_t    sort_value,
                           const bool           defaultSatTabId)
        : direction        (directionArg)
        , center_depth     (depth)
        , open_state       (stateArg)
        , sat_tableId      (satTableId)
        , m_complnum       (complnum)
        , m_CF             (CF)
        , m_Kh             (Kh)
        , m_rw             (rw)
        , m_r0             (r0)
        , m_re             (re)
        , m_connection_length(connection_length)
        , m_skin_factor    (skin_factor)
        , m_d_factor       (d_factor)
        , m_Ke             (Ke)
        , ijk              { i, j, k }
        , m_ctfkind        (ctf_kind)
        , m_global_index   (global_index)
        , m_sort_value     (sort_value)
        , m_defaultSatTabId(defaultSatTabId)
    {}

    Connection::Connection(const RestartIO::RstConnection& rst_connection,
                           const ScheduleGrid&             grid,
                           const FieldPropsManager&        fp)
        : direction        (rst_connection.dir)
        , center_depth     (rst_connection.depth)
        , open_state       (rst_connection.state)
        , sat_tableId      (rst_connection.drain_sat_table)
        , m_complnum       (rst_connection.completion)
        , m_CF             (rst_connection.cf)
        , m_Kh             (rst_connection.kh)
        , m_rw             (rst_connection.diameter / 2)
        , m_r0             (rst_connection.r0)
        , m_re             (0.0)
        , m_connection_length(0.0)
        , m_skin_factor    (rst_connection.skin_factor)
        , m_d_factor       (0.0)
        , m_Ke             (0.0)
        , ijk              (rst_connection.ijk)
        , m_ctfkind        (rst_connection.cf_kind)
        , m_global_index   (grid.get_cell(this->ijk[0], this->ijk[1], this->ijk[2]).global_index)
        , m_sort_value     (rst_connection.rst_index)
        , m_defaultSatTabId(restartDefaultSatTabId)
        , segment_number   (rst_connection.segment)
    {
        if (this->m_defaultSatTabId) {
            const auto active_index = grid
                .get_cell(this->ijk[0], this->ijk[1], this->ijk[2])
                .active_index();

            this->sat_tableId = fp.get_int("SATNUM")[active_index];
        }

        if (this->segment_number > 0) {
            this->m_perf_range = std::make_pair(rst_connection.segdist_start,
                                                rst_connection.segdist_end);
        }

        //TODO recompute re and perf_length from the grid
    }

    Connection Connection::serializationTestObject()
    {
        Connection result;
        result.direction = Direction::Y;
        result.center_depth = 1.0;
        result.open_state = State::OPEN;
        result.sat_tableId = 2;
        result.m_complnum = 3;
        result.m_CF = 4.0;
        result.m_Kh = 5.0;
        result.m_rw = 6.0;
        result.m_r0 = 7.0;
        result.m_re = 7.1;
        result.m_connection_length = 7.2;
        result.m_skin_factor = 8.0;
        result.m_d_factor = 8.5;
        result.m_Ke = 8.9;
        result.ijk = {9, 10, 11};
        result.m_ctfkind = CTFKind::Defaulted;
        result.m_global_index = 12;
        result.m_perf_range = std::make_pair(14,15);
        result.m_injmult = InjMult::serializationTestObject();
        result.m_sort_value = 14;
        result.m_defaultSatTabId = true;
        result.segment_number = 16;
        result.m_subject_to_welpi = true;
        result.m_filter_cake = FilterCake::serializationTestObject();

        return result;
    }

    bool Connection::sameCoordinate(const int i, const int j, const int k) const
    {
        return this->ijk == std::array { i, j, k };
    }

    int Connection::getI() const
    {
        return ijk[0];
    }

    int Connection::getJ() const
    {
        return ijk[1];
    }

    int Connection::getK() const
    {
        return ijk[2];
    }

    std::size_t Connection::global_index() const
    {
        return this->m_global_index;
    }

    bool Connection::attachedToSegment() const
    {
        return this->segment_number > 0;
    }

    std::size_t Connection::sort_value() const
    {
        return m_sort_value;
    }

    bool Connection::getDefaultSatTabId() const
    {
        return m_defaultSatTabId;
    }

    Connection::Direction Connection::dir() const
    {
        return this->direction;
    }

    const std::optional<std::pair<double, double>>&
    Connection::perf_range() const
    {
        return this->m_perf_range;
    }

    void Connection::setDefaultSatTabId(bool id)
    {
        m_defaultSatTabId = id;
    }

    double Connection::depth() const
    {
        return this->center_depth;
    }

    Connection::State Connection::state() const
    {
        return this->open_state;
    }

    int Connection::satTableId() const
    {
        return this->sat_tableId;
    }

    int Connection::complnum() const
    {
        return this->m_complnum;
    }

    void Connection::setComplnum(int complnum)
    {
        this->m_complnum = complnum;
    }

    void Connection::setSkinFactor(double skin_factor)
    {
        this->m_skin_factor = skin_factor;
    }

    void Connection::setDFactor(double d_factor)
    {
        this->m_d_factor = d_factor;
    }

    void Connection::setKe(double Ke)
    {
        this->m_Ke = Ke;
    }

    void Connection::setCF(double CF)
    {
        this->m_CF = CF;
    }

    double Connection::CF() const
    {
        return this->m_CF;
    }

    double Connection::wpimult() const
    {
        return this->m_wpimult;
    }

    double Connection::Kh() const
    {
        return this->m_Kh;
    }

    double Connection::rw() const
    {
        return this->m_rw;
    }

    double Connection::r0() const
    {
        return this->m_r0;
    }

    double Connection::re() const
    {
        return this->m_re;
    }

    double Connection::connectionLength() const
    {
        return this->m_connection_length;
    }

    double Connection::skinFactor() const
    {
        return this->m_skin_factor;
    }

    double Connection::dFactor() const
    {
        return this->m_d_factor;
    }

    double Connection::Ke() const
    {
        return this->m_Ke;
    }

    void Connection::setState(State state)
    {
        this->open_state = state;
    }

    void Connection::updateSegment(const int segment_number_arg,
                                   const double center_depth_arg,
                                   const std::size_t compseg_insert_index,
                                   const std::pair<double, double>& perf_range)
    {
        this->segment_number = segment_number_arg;
        this->center_depth = center_depth_arg;
        this->m_sort_value = compseg_insert_index;
        this->m_perf_range = perf_range;
    }

    void Connection::updateSegmentRST(const int segment_number_arg,
                                      const double center_depth_arg)
    {
        this->segment_number = segment_number_arg;
        this->center_depth = center_depth_arg;
    }

    int Connection::segment() const
    {
        return this->segment_number;
    }

    void Connection::scaleWellPi(double wellPi)
    {
        this->m_wpimult *= wellPi;
        this->m_CF *= wellPi;
    }

    bool Connection::prepareWellPIScaling()
    {
        const auto update = !this->m_subject_to_welpi;

        this->m_subject_to_welpi = true;

        return update;
    }

    bool Connection::applyWellPIScaling(const double scaleFactor)
    {
        if (! this->m_subject_to_welpi) {
            return false;
        }

        this->scaleWellPi(scaleFactor);
        return true;
    }

    std::string Connection::str() const
    {
        std::stringstream ss;
        ss << "ijk: " << this->ijk[0] << ','  << this->ijk[1] << ',' << this->ijk[2] << '\n'
           << "COMPLNUM " << this->m_complnum << '\n'
           << "CF " << this->CF() << '\n'
           << "RW " << this->rw() << '\n'
           << "R0 " << this->r0() << '\n'
           << "Re " << this->re() << '\n'
           << "connection length " << this->connectionLength() << '\n'
           << "skinf " << this->skinFactor() << '\n'
           << "dfactor " << this->dFactor() << '\n'
           << "Ke " << this->Ke() << '\n'
           << "kh " << this->Kh() << '\n'
           << "sat_tableId " << this->sat_tableId << '\n'
           << "open_state " << Connection::State2String(this->open_state) << '\n'
           << "direction " << Connection::Direction2String(this->direction) << '\n'
           << "CTF Source " << Connection::CTFKindToString(this->m_ctfkind) << '\n'
           << "segment_nr " << this->segment_number << '\n'
           << "center_depth " << this->center_depth << '\n'
           << "sort_value" << this->m_sort_value<< '\n';

        if (this->m_injmult.has_value()) {
            ss << "INJMULT " << InjMult::InjMultToString(this->m_injmult.value()) << '\n';
        }

        if (this->m_filter_cake.has_value()) {
            ss << "FilterCake " << FilterCake::filterCakeToString(this->m_filter_cake.value()) << '\n';
        }

        return ss.str();
    }

    bool Connection::operator==(const Connection& that) const
    {
        return (this->direction == that.direction)
            && (this->open_state == that.open_state)
            && (this->sat_tableId == that.sat_tableId)
            && (this->m_complnum == that.m_complnum)
            && (this->m_ctfkind == that.m_ctfkind)
            && (this->m_global_index == that.m_global_index)
            && (this->m_sort_value == that.m_sort_value)
            && (this->m_defaultSatTabId == that.m_defaultSatTabId)
            && (this->segment_number == that.segment_number)
            && (this->m_subject_to_welpi == that.m_subject_to_welpi)
            && (this->ijk == that.ijk)
            && (this->m_injmult == that.m_injmult)
            && (this->center_depth == that.center_depth)
            && (this->m_perf_range == that.m_perf_range)
            && (this->m_filter_cake == that.m_filter_cake)
            && (this->m_CF == that.m_CF)
            && (this->m_rw == that.m_rw)
            && (this->m_r0 == that.m_r0)
            && (this->m_re == that.m_re)
            && (this->m_connection_length == that.m_connection_length)
            && (this->m_skin_factor == that.m_skin_factor)
            && (this->m_d_factor == that.m_d_factor)
            && (this->m_Ke == that.m_Ke)
            && (this->m_Kh == that.m_Kh)
            ;
    }

std::string Connection::State2String(State enumValue)
{
    switch (enumValue) {
    case State::OPEN:
        return "OPEN";

    case State::AUTO:
        return "AUTO";

    case State::SHUT:
        return "SHUT";

    default:
        throw std::invalid_argument {
            "Unhandled Connnection::State value " +
            std::to_string(static_cast<int>(enumValue))
        };
    }
}

Connection::State
Connection::StateFromString(std::string_view stringValue)
{
    if (stringValue == "OPEN")
        return State::OPEN;

    if (stringValue == "SHUT")
        return State::SHUT;

    if (stringValue == "STOP")
        return State::SHUT;

    if (stringValue == "AUTO")
        return State::AUTO;

    throw std::invalid_argument {
        "Unknown Connection::State string: " + std::string { stringValue }
    };
}

std::string Connection::Direction2String(const Direction enumValue)
{
    std::string stringValue;

    switch (enumValue) {
    case Direction::X:
        stringValue = "X";
        break;

    case Direction::Y:
        stringValue = "Y";
        break;

    case Direction::Z:
        stringValue = "Z";
        break;

    default:
        stringValue = std::to_string(static_cast<int>(enumValue));
        break;
    }

    return stringValue;
}

Connection::Direction Connection::DirectionFromString(std::string_view s)
{
    Direction direction;

    if      ((s == "X") || (s == "x")) { direction = Direction::X; }
    else if ((s == "Y") || (s == "y")) { direction = Direction::Y; }
    else if ((s == "Z") || (s == "z")) { direction = Direction::Z; }
    else {
        throw std::invalid_argument {
            "Unsupported completion direction "
            + std::string { s }
        };
    }

    return direction;
}

std::string Connection::Order2String(Order enumValue)
{
    switch (enumValue) {
    case Order::DEPTH:
        return "DEPTH";

    case Order::INPUT:
        return "INPUT";

    case Order::TRACK:
        return "TRACK";

    default:
        throw std::invalid_argument {
            "Unhandled Connection::Order value " +
            std::to_string(static_cast<int>(enumValue))
        };
    }
}

Connection::Order Connection::OrderFromString(std::string_view stringValue)
{
    if (stringValue == "DEPTH")
        return Order::DEPTH;

    if (stringValue == "INPUT")
        return Order::INPUT;

    if (stringValue == "TRACK")
        return Order::TRACK;

    throw std::invalid_argument {
        "Unknown Connection::Order string: " + std::string { stringValue }
    };
}

std::string Connection::CTFKindToString(const CTFKind ctf_kind)
{
    switch (ctf_kind) {
    case CTFKind::DeckValue:
        return "DeckValue";

    case CTFKind::Defaulted:
        return "Defaulted";
    }

    throw std::invalid_argument {
        "Unhandled CTF Kind Value: " +
        std::to_string(static_cast<int>(ctf_kind))
    };
}

Connection::CTFKind Connection::kind() const
{
    return m_ctfkind;
}

const InjMult& Connection::injmult() const
{
    assert(this->activeInjMult());
    return m_injmult.value();
}

bool Connection::activeInjMult() const
{
    return this->m_injmult.has_value();
}

void Connection::setInjMult(const InjMult& inj_mult)
{
    m_injmult = inj_mult;
}

void Connection::setFilterCake(const FilterCake& filter_cake)
{
    this->m_filter_cake = filter_cake;
}

bool Connection::filterCakeActive() const
{
    return this->m_filter_cake.has_value();
}

const FilterCake& Connection::getFilterCake() const
{
    assert(this->filterCakeActive());
    return this->m_filter_cake.value();
}

double Connection::getFilterCakeRadius() const
{
    if (const auto& radius = this->getFilterCake().radius; radius.has_value()) {
        return *radius;
    } else {
        return this->rw();
    }
}

double Connection::getFilterCakeArea() const
{
    if (const auto& flow_area = this->getFilterCake().flow_area; flow_area.has_value()) {
        return *flow_area;
    } else {
        constexpr double two_pi = 2 * 3.14159265358979323846264;
        return two_pi * this->getFilterCakeRadius() * this->connectionLength();
    }
}

} // end of namespace Opm
