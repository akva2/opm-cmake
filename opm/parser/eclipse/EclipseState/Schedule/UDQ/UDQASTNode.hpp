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

#ifndef UDQASTNODE_HPP
#define UDQASTNODE_HPP

#include <string>
#include <set>
#include <vector>

#include <opm/parser/eclipse/EclipseState/Schedule/UDQ/UDQSet.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/UDQ/UDQContext.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/UDQ/UDQEnums.hpp>



namespace Opm {

class UDQASTNode {
public:
    UDQASTNode();
    UDQASTNode(UDQTokenType type_arg);
    UDQASTNode(double scalar_value);
    UDQASTNode(UDQTokenType type_arg, const std::string& string_value, const std::vector<std::string>& selector);
    UDQASTNode(UDQTokenType type_arg, const std::string& func_name, const UDQASTNode& arg);
    UDQASTNode(UDQTokenType type_arg, const std::string& func_name, const UDQASTNode& left, const UDQASTNode& right);
    UDQASTNode(UDQTokenType typ, UDQVarType varType,
               const std::string& stringVal,
               double scalarVal,
               const std::vector<std::string>& selectors,
               const std::vector<UDQASTNode>& argList);

    UDQSet eval(UDQVarType eval_target, const UDQContext& context) const;

    UDQTokenType type;
    UDQVarType var_type = UDQVarType::NONE;
    std::set<UDQTokenType> func_tokens() const;

    bool operator==(const UDQASTNode& data) const;

    const std::string& stringValue() const;
    double scalarValue() const;
    const std::vector<std::string>& getSelectors() const;
    const std::vector<UDQASTNode>& getArgList() const;

private:
    void func_tokens(std::set<UDQTokenType>& tokens) const;

    std::string string_value;
    double scalar_value;
    std::vector<std::string> selector;
    std::vector<UDQASTNode> arglist;
};

}

#endif
