// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*
  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.

  Consult the COPYING file in the top-level source directory of this
  module for the precise wording of the license and the list of
  copyright holders.
*/
/*!
 * \file
 *
 * \brief This test makes sure that the programming interface is
 *        observed by all fluid systems
 */
#include "config.h"

#include <boost/mpl/list.hpp>

#define BOOST_TEST_MODULE FluidSystems
#include <boost/test/unit_test.hpp>

#include <opm/material/checkFluidSystem.hpp>

#include <opm/material/components/C1.hpp>
#include <opm/material/components/C10.hpp>
#include <opm/material/components/N2.hpp>
#include <opm/material/components/SimpleCO2.hpp>

#include <opm/material/densead/Evaluation.hpp>
#include <opm/material/densead/Math.hpp>

// include all fluid systems in opm-material
#include <opm/material/fluidsystems/SinglePhaseFluidSystem.hpp>
#include <opm/material/fluidsystems/TwoPhaseImmiscibleFluidSystem.hpp>
#include <opm/material/fluidsystems/BlackOilFluidSystem.hpp>
#include <opm/material/fluidsystems/BrineCO2FluidSystem.hpp>
#include <opm/material/fluidsystems/GenericOilGasFluidSystem.hpp>
#include <opm/material/fluidsystems/H2ON2FluidSystem.hpp>
#include <opm/material/fluidsystems/H2ON2LiquidPhaseFluidSystem.hpp>
#include <opm/material/fluidsystems/H2OAirFluidSystem.hpp>
#include <opm/material/fluidsystems/H2OAirMesityleneFluidSystem.hpp>
#include <opm/material/fluidsystems/H2OAirXyleneFluidSystem.hpp>
#include <opm/material/fluidsystems/ThreeComponentFluidSystem.hh>

#include <opm/material/thermal/FluidThermalConductionLaw.hpp>

// include all fluid states
#include <opm/material/fluidstates/PressureOverlayFluidState.hpp>
#include <opm/material/fluidstates/SaturationOverlayFluidState.hpp>
#include <opm/material/fluidstates/TemperatureOverlayFluidState.hpp>
#include <opm/material/fluidstates/CompositionalFluidState.hpp>
#include <opm/material/fluidstates/NonEquilibriumFluidState.hpp>
#include <opm/material/fluidstates/ImmiscibleFluidState.hpp>
#include <opm/material/fluidstates/SimpleModularFluidState.hpp>
#include <opm/material/fluidstates/BlackOilFluidState.hpp>

// include the tables for CO2 which are delivered with opm-material by default
#include <opm/material/common/UniformTabulated2DFunction.hpp>

#include <opm/input/eclipse/Python/Python.hpp>
#if HAVE_ECL_INPUT
#include <opm/input/eclipse/Deck/Deck.hpp>
#include <opm/input/eclipse/EclipseState/EclipseState.hpp>
#include <opm/input/eclipse/Schedule/Schedule.hpp>
#endif

// check that the blackoil fluid system implements all non-standard functions
template <class Evaluation, class FluidSystem>
void ensureBlackoilApi(std::shared_ptr<FluidSystem> fluidSystem)
{
    // here we don't want to call these methods at runtime, we just want to make sure
    // that they compile
    while (false) {
#if HAVE_ECL_INPUT
        auto python = std::make_shared<Opm::Python>();
        Opm::Deck deck;
        Opm::EclipseState eclState(deck);
        Opm::Schedule schedule(deck, eclState, python);
        fluidSystem->initFromState(eclState, schedule);
#endif

        typedef typename FluidSystem::Scalar Scalar;
        typedef Opm::BlackOilFluidState<Evaluation, FluidSystem> FluidState;
        FluidState fluidState(fluidSystem);
        Evaluation XoG = 0.0;
        Evaluation XwG = 0.0;
        Evaluation XgO = 0.0;
        Evaluation Rs = 0.0;
        Evaluation Rv = 0.0;

        // some additional typedefs
        using OilPvt = typename FluidSystem::OilPvt;
        using GasPvt = typename FluidSystem::GasPvt;
        using WaterPvt = typename FluidSystem::WaterPvt;

        // check the black-oil specific enums
        static_assert(FluidSystem::numPhases == 3, "");
        static_assert(FluidSystem::numComponents == 3, "");

        static_assert(0 <= FluidSystem::oilPhaseIdx && FluidSystem::oilPhaseIdx < 3, "");
        static_assert(0 <= FluidSystem::gasPhaseIdx && FluidSystem::gasPhaseIdx < 3, "");
        static_assert(0 <= FluidSystem::waterPhaseIdx && FluidSystem::waterPhaseIdx < 3, "");

        static_assert(0 <= FluidSystem::oilCompIdx && FluidSystem::oilCompIdx < 3, "");
        static_assert(0 <= FluidSystem::gasCompIdx && FluidSystem::gasCompIdx < 3, "");
        static_assert(0 <= FluidSystem::waterCompIdx && FluidSystem::waterCompIdx < 3, "");

        // check the non-parser initialization
        std::shared_ptr<OilPvt> oilPvt;
        std::shared_ptr<GasPvt> gasPvt;
        std::shared_ptr<WaterPvt> waterPvt;

        unsigned numPvtRegions = 2;
        fluidSystem->initBegin(numPvtRegions);
        fluidSystem->setEnableDissolvedGas(true);
        fluidSystem->setEnableVaporizedOil(true);
        fluidSystem->setEnableDissolvedGasInWater(true);
        fluidSystem->setGasPvt(gasPvt);
        fluidSystem->setOilPvt(oilPvt);
        fluidSystem->setWaterPvt(waterPvt);
        fluidSystem->setReferenceDensities(/*oil=*/600.0,
                                           /*water=*/1000.0,
                                           /*gas=*/1.0,
                                           /*regionIdx=*/0);
        fluidSystem->initEnd();

        // the molarMass() method has an optional argument for the PVT region
        [[maybe_unused]] unsigned numRegions = fluidSystem->numRegions();
        [[maybe_unused]] Scalar Mg = fluidSystem->molarMass(FluidSystem::gasCompIdx,
                                                            /*regionIdx=*/0);
        [[maybe_unused]] bool b1 = fluidSystem->enableDissolvedGas();
        [[maybe_unused]] bool b2 = fluidSystem->enableVaporizedOil();
        [[maybe_unused]] Scalar rhoRefOil = fluidSystem->referenceDensity(FluidSystem::oilPhaseIdx,
                                                                          /*regionIdx=*/0);
        std::cout << fluidSystem->convertXoGToRs(XoG, /*regionIdx=*/0);
        std::cout << fluidSystem->convertXwGToRsw(XwG, /*regionIdx=*/0);
        std::cout << fluidSystem->convertXgOToRv(XgO, /*regionIdx=*/0);
        std::cout << fluidSystem->convertXoGToxoG(XoG, /*regionIdx=*/0);
        std::cout << fluidSystem->convertXgOToxgO(XgO, /*regionIdx=*/0);
        std::cout << fluidSystem->convertRsToXoG(Rs, /*regionIdx=*/0);
        std::cout << fluidSystem->convertRvToXgO(Rv, /*regionIdx=*/0);

        for (unsigned phaseIdx = 0; phaseIdx < FluidSystem::numPhases; ++ phaseIdx) {
            std::cout << fluidSystem->density(fluidState, phaseIdx, /*regionIdx=*/0);
            std::cout << fluidSystem->saturatedDensity(fluidState, phaseIdx, /*regionIdx=*/0);
            std::cout << fluidSystem->inverseFormationVolumeFactor(fluidState, phaseIdx, /*regionIdx=*/0);
            std::cout << fluidSystem->saturatedInverseFormationVolumeFactor(fluidState, phaseIdx, /*regionIdx=*/0);
            std::cout << fluidSystem->viscosity(fluidState, phaseIdx, /*regionIdx=*/0);
            std::cout << fluidSystem->saturatedDissolutionFactor(fluidState, phaseIdx, /*regionIdx=*/0);
            std::cout << fluidSystem->saturatedDissolutionFactor(fluidState, phaseIdx, /*regionIdx=*/0, /*maxSo=*/1.0);
            std::cout << fluidSystem->saturationPressure(fluidState, phaseIdx, /*regionIdx=*/0);
            for (unsigned compIdx = 0; compIdx < fluidSystem->numComponents; ++ compIdx)
                std::cout << fluidSystem->fugacityCoefficient(fluidState, phaseIdx, compIdx,  /*regionIdx=*/0);
        }

        // the "not considered safe to use directly" API
        [[maybe_unused]] const OilPvt& oilPvt2 = fluidSystem->oilPvt();
        [[maybe_unused]] const GasPvt& gasPvt2 = fluidSystem->gasPvt();
        [[maybe_unused]] const WaterPvt& waterPvt2 = fluidSystem->waterPvt();
    }
}

using EvalTypes = boost::mpl::list<float,double,Opm::DenseAd::Evaluation<float,3>,Opm::DenseAd::Evaluation<double,3>>;

BOOST_AUTO_TEST_CASE_TEMPLATE(SimpleModularFluidState, Eval, EvalTypes)
{
    Opm::SimpleModularFluidState<Eval,
                                 /*numPhases=*/2,
                                 /*numComponents=*/0,
                                 /*FluidSystem=*/void,
                                 /*storePressure=*/false,
                                 /*storeTemperature=*/false,
                                 /*storeComposition=*/false,
                                 /*storeFugacity=*/false,
                                 /*storeSaturation=*/false,
                                 /*storeDensity=*/false,
                                 /*storeViscosity=*/false,
                                 /*storeEnthalpy=*/false> fs;
    checkFluidState<Eval>(fs);

    using FluidSystem = Opm::H2ON2FluidSystem<Eval>;
    Opm::SimpleModularFluidState<Eval,
                                 /*numPhases=*/2,
                                 /*numComponents=*/2,
                                 FluidSystem,
                                 /*storePressure=*/true,
                                 /*storeTemperature=*/true,
                                 /*storeComposition=*/true,
                                 /*storeFugacity=*/true,
                                 /*storeSaturation=*/true,
                                 /*storeDensity=*/true,
                                 /*storeViscosity=*/true,
                                 /*storeEnthalpy=*/true> fs2;

        checkFluidState<Eval>(fs2);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(CompositionalFluidState, Eval, EvalTypes)
{
    using FluidSystem = Opm::H2ON2FluidSystem<Eval>;
    Opm::CompositionalFluidState<Eval, FluidSystem> fs;
    checkFluidState<Eval>(fs);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(NonEquilibriumFluidState, Eval, EvalTypes)
{
    using FluidSystem = Opm::H2ON2FluidSystem<Eval>;
    Opm::NonEquilibriumFluidState<Eval, FluidSystem> fs;
    checkFluidState<Eval>(fs);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(ImmiscibleFluidState, Eval, EvalTypes)
{
    using FluidSystem = Opm::H2ON2FluidSystem<Eval>;
    Opm::ImmiscibleFluidState<Eval, FluidSystem> fs;
    checkFluidState<Eval>(fs);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(TemperatureOverlayFluidState, Eval, EvalTypes)
{
    using FluidSystem = Opm::H2ON2FluidSystem<Eval>;
    using BaseFluidState = Opm::CompositionalFluidState<Eval, FluidSystem>;
    BaseFluidState baseFs;
    Opm::TemperatureOverlayFluidState fs(baseFs);
    checkFluidState<Eval>(fs);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(PressureOverlayFluidState, Eval, EvalTypes)
{
    using FluidSystem = Opm::H2ON2FluidSystem<Eval>;
    using BaseFluidState = Opm::CompositionalFluidState<Eval, FluidSystem>;
    BaseFluidState baseFs;
    Opm::PressureOverlayFluidState fs(baseFs);
    checkFluidState<Eval>(fs);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(SaturationOverlayFluidState, Eval, EvalTypes)
{
    using FluidSystem = Opm::H2ON2FluidSystem<Eval>;
    using BaseFluidState = Opm::CompositionalFluidState<Eval, FluidSystem>;
    BaseFluidState baseFs;
    Opm::SaturationOverlayFluidState fs(baseFs);
    checkFluidState<Eval>(fs);
}

using ScalarTypes = boost::mpl::list<float,double>;

BOOST_AUTO_TEST_CASE_TEMPLATE(BlackoilFluidSystem, Scalar, ScalarTypes)
{
    using Evaluation = Opm::DenseAd::Evaluation<Scalar,3>;
    using FluidSystem = Opm::BlackOilFluidSystem<Scalar>;

    auto fluidSystem = std::make_shared<FluidSystem>();

    if (false) checkFluidSystem<Scalar, FluidSystem, Scalar, Scalar>(*fluidSystem);
    if (false) checkFluidSystem<Scalar, FluidSystem, Evaluation, Scalar>(*fluidSystem);
    if (false) checkFluidSystem<Scalar, FluidSystem, Evaluation, Evaluation>(*fluidSystem);

    using BlackoilDummyEval = Opm::DenseAd::Evaluation<Scalar, 1>;
    ensureBlackoilApi<Scalar, FluidSystem>(fluidSystem);
    ensureBlackoilApi<BlackoilDummyEval, FluidSystem>(fluidSystem);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(BrineCO2FluidSystem, Scalar, ScalarTypes)
{
    using Evaluation = Opm::DenseAd::Evaluation<Scalar,3>;
    using FluidSystem = Opm::BrineCO2FluidSystem<Scalar>;

    FluidSystem fluidSystem;

    checkFluidSystem<Scalar, FluidSystem, Scalar, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Evaluation>(fluidSystem);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(H2ON2FluidSystem, Scalar, ScalarTypes)
{
    using Evaluation = Opm::DenseAd::Evaluation<Scalar,3>;
    using FluidSystem = Opm::H2ON2FluidSystem<Scalar>;
    FluidSystem fluidSystem;

    checkFluidSystem<Scalar, FluidSystem, Scalar, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Evaluation>(fluidSystem);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(H2ON2LiquidPhaseFluidSystem, Scalar, ScalarTypes)
{
    using Evaluation = Opm::DenseAd::Evaluation<Scalar,3>;
    using FluidSystem = Opm::H2ON2LiquidPhaseFluidSystem<Scalar>;
    FluidSystem fluidSystem;

    checkFluidSystem<Scalar, FluidSystem, Scalar, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Evaluation>(fluidSystem);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(H2OAirFluidSystem, Scalar, ScalarTypes)
{
    using Evaluation = Opm::DenseAd::Evaluation<Scalar,3>;
    using H2O = Opm::SimpleH2O<Scalar>;
    using FluidSystem = Opm::H2OAirFluidSystem<Scalar, H2O>;
    FluidSystem fluidSystem;

    checkFluidSystem<Scalar, FluidSystem, Scalar, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Evaluation>(fluidSystem);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(H2OAirXyleneFluidSystem, Scalar, ScalarTypes)
{
    using Evaluation = Opm::DenseAd::Evaluation<Scalar,3>;
    using FluidSystem = Opm::H2OAirXyleneFluidSystem<Scalar>;
    FluidSystem fluidSystem;

    checkFluidSystem<Scalar, FluidSystem, Scalar, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Evaluation>(fluidSystem);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(TwoPhaseImmiscibleFluidSystemLL, Scalar, ScalarTypes)
{
    using Evaluation = Opm::DenseAd::Evaluation<Scalar,3>;
    using Liquid = Opm::LiquidPhase<Scalar, Opm::H2O<Scalar>>;
    using FluidSystem = Opm::TwoPhaseImmiscibleFluidSystem<Scalar, Liquid, Liquid>;
    FluidSystem fluidSystem;

    checkFluidSystem<Scalar, FluidSystem, Scalar, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Evaluation>(fluidSystem);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(TwoPhaseImmiscibleFluidSystemLG, Scalar, ScalarTypes)
{
    using Evaluation = Opm::DenseAd::Evaluation<Scalar,3>;
    using Gas = Opm::GasPhase<Scalar, Opm::N2<Scalar>>;
    using Liquid = Opm::LiquidPhase<Scalar, Opm::H2O<Scalar>>;
    using FluidSystem = Opm::TwoPhaseImmiscibleFluidSystem<Scalar, Liquid, Gas>;
    FluidSystem fluidSystem;

    checkFluidSystem<Scalar, FluidSystem, Scalar, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Evaluation>(fluidSystem);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(TwoPhaseImmiscibleFluidSystemGL, Scalar, ScalarTypes)
{
    using Evaluation = Opm::DenseAd::Evaluation<Scalar,3>;
    using Gas = Opm::GasPhase<Scalar, Opm::N2<Scalar>>;
    using Liquid = Opm::LiquidPhase<Scalar, Opm::H2O<Scalar>>;
    using FluidSystem = Opm::TwoPhaseImmiscibleFluidSystem<Scalar, Gas, Liquid>;
    FluidSystem fluidSystem;

    checkFluidSystem<Scalar, FluidSystem, Scalar, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Evaluation>(fluidSystem);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(SinglePhaseFluidSystemL, Scalar, ScalarTypes)
{
    using Evaluation = Opm::DenseAd::Evaluation<Scalar,3>;
    using Liquid = Opm::LiquidPhase<Scalar, Opm::H2O<Scalar>>;
    using FluidSystem = Opm::SinglePhaseFluidSystem<Scalar, Liquid>;
    FluidSystem fluidSystem;

    checkFluidSystem<Scalar, FluidSystem, Scalar, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Evaluation>(fluidSystem);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(SinglePhaseFluidSystemG, Scalar, ScalarTypes)
{
    using Evaluation = Opm::DenseAd::Evaluation<Scalar,3>;
    using Gas = Opm::GasPhase<Scalar, Opm::N2<Scalar>>;
    using FluidSystem = Opm::SinglePhaseFluidSystem<Scalar, Gas>;
    FluidSystem fluidSystem;

    checkFluidSystem<Scalar, FluidSystem, Scalar, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Evaluation>(fluidSystem);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(ThreeComponentFluidSystem, Scalar, ScalarTypes)
{
    using Evaluation = Opm::DenseAd::Evaluation<Scalar,3>;
    using FluidSystem = Opm::ThreeComponentFluidSystem<Scalar>;
    FluidSystem fluidSystem;

    checkFluidSystem<Scalar, FluidSystem, Scalar, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Evaluation>(fluidSystem);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(GenericFluidSystem, Scalar, ScalarTypes)
{
    using Evaluation = Opm::DenseAd::Evaluation<Scalar, 4>;
    using FluidSystem = Opm::GenericOilGasFluidSystem<Scalar, 4>;
    FluidSystem fluidSystem;

    using CompParm = typename FluidSystem::ComponentParam;
    using CO2 = Opm::SimpleCO2<Scalar>;
    using C1 = Opm::C1<Scalar>;
    using N2 = Opm::N2<Scalar>;
    using C10 = Opm::C10<Scalar>;
    fluidSystem.addComponent(CompParm {CO2::name(), CO2::molarMass(), CO2::criticalTemperature(),
                                       CO2::criticalPressure(), CO2::criticalVolume(), CO2::acentricFactor()});
    fluidSystem.addComponent(CompParm {C1::name(), C1::molarMass(), C1::criticalTemperature(),
                                       C1::criticalPressure(), C1::criticalVolume(), C1::acentricFactor()});
    fluidSystem.addComponent(CompParm{C10::name(), C10::molarMass(), C10::criticalTemperature(),
                                      C10::criticalPressure(), C10::criticalVolume(), C10::acentricFactor()});
    fluidSystem.addComponent(CompParm{N2::name(), N2::molarMass(), N2::criticalTemperature(),
                                      N2::criticalPressure(), N2::criticalVolume(), N2::acentricFactor()});

    checkFluidSystem<Scalar, FluidSystem, Scalar, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Scalar>(fluidSystem);
    checkFluidSystem<Scalar, FluidSystem, Evaluation, Evaluation>(fluidSystem);
}
