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

#ifndef UDQINPUT__HPP_
#define UDQINPUT__HPP_

#include <opm/input/eclipse/Schedule/UDQ/UDQAssign.hpp>
#include <opm/input/eclipse/Schedule/UDQ/UDQDefine.hpp>
#include <opm/input/eclipse/Schedule/UDQ/UDQEnums.hpp>

#include <cstddef>
#include <string>
#include <variant>

namespace Opm {

class UDQIndex
{
public:
    UDQIndex() = default;

    UDQIndex(const std::size_t insert_index_arg,
             const std::size_t typed_insert_index_arg,
             const UDQAction   action_arg,
             const UDQVarType  var_type_arg)
        : insert_index      (insert_index_arg)
        , typed_insert_index(typed_insert_index_arg)
        , action            (action_arg)
        , var_type          (var_type_arg)
    {}

    static UDQIndex serializationTestObject()
    {
        UDQIndex result;
        result.insert_index = 1;
        result.typed_insert_index = 2;
        result.action = UDQAction::ASSIGN;
        result.var_type = UDQVarType::WELL_VAR;

        return result;
    }

    bool operator==(const UDQIndex& data) const
    {
        return (insert_index == data.insert_index)
            && (typed_insert_index == data.typed_insert_index)
            && (action == data.action)
            && (var_type == data.var_type)
            ;
    }

    template <class Serializer>
    void serializeOp(Serializer& serializer)
    {
        serializer(insert_index);
        serializer(typed_insert_index);
        serializer(action);
        serializer(var_type);
    }

    std::size_t insert_index{};
    std::size_t typed_insert_index{};
    UDQAction action{UDQAction::ASSIGN};
    UDQVarType var_type{UDQVarType::NONE};
};

class UDQInput
{
public:
    UDQInput(const UDQIndex& index, const UDQDefine* udq_define, const std::string& unit);
    UDQInput(const UDQIndex& index, const UDQAssign* udq_assign, const std::string& unit);

    template <typename T>
    const T& get() const;

    template <typename T>
    bool is() const;

    const std::string& keyword()  const { return this->m_keyword; }
    const std::string& unit()     const { return this->m_unit; }
    UDQVarType         var_type() const { return this->m_var_type; }
    const UDQIndex index;

    bool operator==(const UDQInput& other) const;

private:
    std::variant<const UDQDefine*, const UDQAssign*> value{};
    std::string m_keyword{};
    UDQVarType m_var_type{UDQVarType::NONE};
    std::string m_unit{};
};

} // namespace Opm

#endif // UDQINPUT__HPP_
