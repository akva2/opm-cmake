/*
  Copyright 2020 Equinor ASA.

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

#ifndef PAVE_HPP
#define PAVE_HPP

#include <ostream>

namespace Opm {
    class DeckRecord;
} // Namespace Opm

namespace Opm {

class PAvg
{
public:
    enum class DepthCorrection
    {
        WELL = 1,
        RES  = 2,
        NONE = 3,
    };

    PAvg();
    explicit PAvg(const DeckRecord& record);
    PAvg(double          inner_weight,
         double          conn_weight,
         DepthCorrection depth_correction,
         bool            use_open_connections);

    static PAvg serializationTestObject();

    double inner_weight() const
    {
        return this->m_inner_weight;
    }

    double conn_weight() const
    {
        return this->m_conn_weight;
    }

    bool open_connections() const
    {
        return this->m_open_connections;
    }

    DepthCorrection depth_correction() const
    {
        return this->m_depth_correction;
    }

    bool use_porv() const;

    template <class Serializer>
    void serializeOp(Serializer& serializer)
    {
        serializer(this->m_inner_weight);
        serializer(this->m_conn_weight);
        serializer(this->m_depth_correction);
        serializer(this->m_open_connections);
    }

    bool operator==(const PAvg& other) const;
    bool operator!=(const PAvg& other) const;

    void print(std::ostream& out) const
    {
        out << m_inner_weight << " " << m_conn_weight
            << " " << static_cast<int>(m_depth_correction)
            << " " << m_open_connections;
    }

private:
    double m_inner_weight;
    double m_conn_weight;
    DepthCorrection m_depth_correction;
    bool m_open_connections;
};

} // namespace Opm

#endif // PAVE_HPP
