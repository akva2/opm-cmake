/*
  Copyright 2014 Statoil ASA.

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


#ifndef BOX_HPP_
#define BOX_HPP_

#include <vector>
#include <cstddef>

namespace Opm {

    class Box {
    public:
        Box() = default;
        Box(int nx , int ny , int nz);
        Box(const Box& globalBox , int i1 , int i2 , int j1 , int j2 , int k1 , int k2); // Zero offset coordinates.
        Box(int nx, int ny, int nz, int i1 , int i2 , int j1 , int j2 , int k1 , int k2);
        size_t size() const;
        bool   isGlobal() const;
        size_t getDim(size_t idim) const;
        const std::vector<size_t>& getIndexList() const;
        bool equal(const Box& other) const;

        explicit operator bool() const;
        std::vector<size_t>::const_iterator begin() const;
        std::vector<size_t>::const_iterator end() const;


        int I1() const;
        int I2() const;
        int J1() const;
        int J2() const;
        int K1() const;
        int K2() const;

    private:
        void initIndexList();
        size_t m_dims[3] = { 0, 0, 0 };
        size_t m_offset[3];
        size_t m_stride[3];

        bool   m_isGlobal;
        std::vector<size_t> m_indexList;

        int lower(int dim) const;
        int upper(int dim) const;
    };
}


#endif
