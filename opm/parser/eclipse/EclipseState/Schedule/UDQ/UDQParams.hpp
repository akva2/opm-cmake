/*
 Copyright 2018 Statoil ASA.

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

#ifndef OPM_UDQ_PARAMS_HPP
#define OPM_UDQ_PARAMS_HPP

#include <random>

namespace Opm {

    class Deck;

    class UDQParams
    {
    public:
        explicit UDQParams(const Deck& deck);
        UDQParams();
        UDQParams(bool reseed, int seed,
                  double range, double undefined,
                  double cmp);

        bool reseed() const;
        int rand_seed() const noexcept;
        void   reseedRNG(int seed);
        double range() const noexcept;
        double undefinedValue() const noexcept;
        double cmpEpsilon() const noexcept;

        std::mt19937& sim_rng();
        std::mt19937& true_rng();

        bool operator==(const UDQParams& data) const;
    private:
        bool reseed_rng;
        int random_seed;
        double value_range;
        double undefined_value;
        double cmp_eps;

        std::mt19937 m_sim_rng;  // The sim_rng is seeded deterministiaclly at simulation start.
        std::mt19937 m_true_rng; // The true_rng is seeded with a "true" random seed; this rng can be reset with reseedRNG()
    };
}

#endif
