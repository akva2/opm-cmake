/*
  Copyright 2015 Statoil ASA.

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
#include <utility>

#include <opm/parser/eclipse/EclipseState/Tables/TableSchema.hpp>

namespace Opm {

    TableSchema::TableSchema(const OrderedMap<std::string, ColumnSchema>& columns) :
        m_columns(columns)
    {
    }

    void TableSchema::addColumn( ColumnSchema column ) {
        m_columns.insert( std::make_pair( column.name(), column ));
    }

    const ColumnSchema& TableSchema::getColumn( const std::string& name ) const {
        return m_columns.get( name );
    }

    const ColumnSchema& TableSchema::getColumn( size_t columnIndex ) const {
        return m_columns.iget( columnIndex );
    }

    size_t TableSchema::size() const {
        return m_columns.size();
    }

    bool TableSchema::hasColumn(const std::string& name) const {
        return m_columns.count( name ) > 0;
    }

    const OrderedMap<std::string, ColumnSchema>& TableSchema::getColumns() const {
        return m_columns;
    }

    bool TableSchema::operator==(const TableSchema& data) const {
        return this->getColumns() == data.getColumns();
    }
}
