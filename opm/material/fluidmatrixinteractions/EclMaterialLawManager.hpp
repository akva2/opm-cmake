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
 * \copydoc Opm::EclMaterialLawManager
 */
#if ! HAVE_ECL_INPUT
#error "Eclipse input support in opm-common is required to use the ECL material manager!"
#endif

#ifndef OPM_ECL_MATERIAL_LAW_MANAGER_HPP
#define OPM_ECL_MATERIAL_LAW_MANAGER_HPP

#include <opm/input/eclipse/EclipseState/Grid/FaceDir.hpp>

#include <opm/material/fluidmatrixinteractions/SatCurveMultiplexer.hpp>
#include <opm/material/fluidmatrixinteractions/EclEpsTwoPhaseLaw.hpp>
#include <opm/material/fluidmatrixinteractions/EclHysteresisConfig.hpp>
#include <opm/material/fluidmatrixinteractions/EclHysteresisTwoPhaseLaw.hpp>
#include <opm/material/fluidmatrixinteractions/EclMultiplexerMaterial.hpp>
#include <opm/material/fluidmatrixinteractions/MaterialTraits.hpp>
#include <opm/material/fluidmatrixinteractions/DirectionalMaterialLawParams.hpp>
#include <opm/input/eclipse/EclipseState/Tables/TableColumn.hpp>
#include <opm/input/eclipse/EclipseState/EclipseState.hpp>

#include <cassert>
#include <memory>
#include <tuple>
#include <vector>

namespace Opm {

class EclipseState;
class EclEpsConfig;
class EclEpsGridProperties;
template<class Scalar> class EclEpsScalingPoints;
template<class Scalar> struct EclEpsScalingPointsInfo;
enum class EclTwoPhaseSystemType;
class Runspec;
class SgfnTable;
class SgofTable;
class SlgofTable;

/*!
 * \ingroup fluidmatrixinteractions
 *
 * \brief Provides an simple way to create and manage the material law objects
 *        for a complete ECL deck.
 */
template <class TraitsT>
class EclMaterialLawManager
{
private:
    using Traits = TraitsT;
    using Scalar = typename Traits::Scalar;
    enum { waterPhaseIdx = Traits::wettingPhaseIdx };
    enum { oilPhaseIdx = Traits::nonWettingPhaseIdx };
    enum { gasPhaseIdx = Traits::gasPhaseIdx };
    enum { numPhases = Traits::numPhases };

    using GasOilTraits = TwoPhaseMaterialTraits<Scalar, oilPhaseIdx, gasPhaseIdx>;
    using OilWaterTraits = TwoPhaseMaterialTraits<Scalar, waterPhaseIdx, oilPhaseIdx>;
    using GasWaterTraits = TwoPhaseMaterialTraits<Scalar, waterPhaseIdx, gasPhaseIdx>;

    // the two-phase material law which is defined on effective (unscaled) saturations
    using GasOilEffectiveTwoPhaseLaw = SatCurveMultiplexer<GasOilTraits>;
    using OilWaterEffectiveTwoPhaseLaw = SatCurveMultiplexer<OilWaterTraits>;
    using GasWaterEffectiveTwoPhaseLaw = SatCurveMultiplexer<GasWaterTraits>;

    using GasOilEffectiveTwoPhaseParams = typename GasOilEffectiveTwoPhaseLaw::Params;
    using OilWaterEffectiveTwoPhaseParams = typename OilWaterEffectiveTwoPhaseLaw::Params;
    using GasWaterEffectiveTwoPhaseParams = typename GasWaterEffectiveTwoPhaseLaw::Params;

    // the two-phase material law which is defined on absolute (scaled) saturations
    using GasOilEpsTwoPhaseLaw = EclEpsTwoPhaseLaw<GasOilEffectiveTwoPhaseLaw>;
    using OilWaterEpsTwoPhaseLaw = EclEpsTwoPhaseLaw<OilWaterEffectiveTwoPhaseLaw>;
    using GasWaterEpsTwoPhaseLaw = EclEpsTwoPhaseLaw<GasWaterEffectiveTwoPhaseLaw>;
    using GasOilEpsTwoPhaseParams = typename GasOilEpsTwoPhaseLaw::Params;
    using OilWaterEpsTwoPhaseParams = typename OilWaterEpsTwoPhaseLaw::Params;
    using GasWaterEpsTwoPhaseParams = typename GasWaterEpsTwoPhaseLaw::Params;

    // the scaled two-phase material laws with hystersis
    using GasOilTwoPhaseLaw = EclHysteresisTwoPhaseLaw<GasOilEpsTwoPhaseLaw>;
    using OilWaterTwoPhaseLaw = EclHysteresisTwoPhaseLaw<OilWaterEpsTwoPhaseLaw>;
    using GasWaterTwoPhaseLaw = EclHysteresisTwoPhaseLaw<GasWaterEpsTwoPhaseLaw>;
    using GasOilTwoPhaseHystParams = typename GasOilTwoPhaseLaw::Params;
    using OilWaterTwoPhaseHystParams = typename OilWaterTwoPhaseLaw::Params;
    using GasWaterTwoPhaseHystParams = typename GasWaterTwoPhaseLaw::Params;

public:
    // the three-phase material law used by the simulation
    using MaterialLaw = EclMultiplexerMaterial<Traits, GasOilTwoPhaseLaw, OilWaterTwoPhaseLaw, GasWaterTwoPhaseLaw>;
    using MaterialLawParams = typename MaterialLaw::Params;
    using DirectionalMaterialLawParamsPtr = std::unique_ptr<DirectionalMaterialLawParams<MaterialLawParams>>;

    EclMaterialLawManager();
    ~EclMaterialLawManager();

    static EclMaterialLawManager<TraitsT> serializationTestObject();

private:
    // internal typedefs
    using GasOilEffectiveParamVector = std::vector<std::shared_ptr<GasOilEffectiveTwoPhaseParams>>;
    using OilWaterEffectiveParamVector = std::vector<std::shared_ptr<OilWaterEffectiveTwoPhaseParams>>;
    using GasWaterEffectiveParamVector = std::vector<std::shared_ptr<GasWaterEffectiveTwoPhaseParams>>;

    using GasOilScalingPointsVector = std::vector<std::shared_ptr<EclEpsScalingPoints<Scalar>>>;
    using OilWaterScalingPointsVector = std::vector<std::shared_ptr<EclEpsScalingPoints<Scalar>>>;
    using GasWaterScalingPointsVector = std::vector<std::shared_ptr<EclEpsScalingPoints<Scalar>>>;
    using OilWaterScalingInfoVector = std::vector<EclEpsScalingPointsInfo<Scalar>>;
    using GasOilParamVector = std::vector<std::shared_ptr<GasOilTwoPhaseHystParams>>;
    using OilWaterParamVector = std::vector<std::shared_ptr<OilWaterTwoPhaseHystParams>>;
    using GasWaterParamVector = std::vector<std::shared_ptr<GasWaterTwoPhaseHystParams>>;
    using MaterialLawParamsVector = std::vector<std::shared_ptr<MaterialLawParams>>;

    // helper classes

    // This class' implementation is defined in "EclMaterialLawManagerInitParams.cpp"
    class InitParams {
    public:
        InitParams(EclMaterialLawManager<TraitsT>& parent, const EclipseState& eclState, size_t numCompressedElems);
        void run();
    private:
        class HystParams;
        void copySatnumArrays_();
        void copyIntArray_(std::vector<int>& dest, const std::string keyword);
        unsigned imbRegion_(std::vector<int>& array, unsigned elemIdx);
        void initArrays_(
            std::vector<std::vector<int>*>& satnumArray,
            std::vector<std::vector<int>*>& imbnumArray,
            std::vector<std::vector<MaterialLawParams>*>& mlpArray);
        void initMaterialLawParamVectors_();
        void initOilWaterScaledEpsInfo_();
        void initSatnumRegionArray_();
        void initThreePhaseParams_(
            HystParams &hystParams,
            MaterialLawParams& materialParams,
            unsigned satRegionIdx,
            unsigned elemIdx);
        void readEffectiveParameters_();
        void readUnscaledEpsPointsVectors_();
        template <class Container>
          void readUnscaledEpsPoints_(Container& dest, std::shared_ptr<EclEpsConfig> config, EclTwoPhaseSystemType system_type);
        unsigned satRegion_(std::vector<int>& array, unsigned elemIdx);
        unsigned satOrImbRegion_(std::vector<int>& array, std::vector<int>& default_vec, unsigned elemIdx);

        // This class' implementation is defined in "EclMaterialLawManagerHystParams.cpp"
        class HystParams {
        public:
            HystParams(EclMaterialLawManager<TraitsT>::InitParams& init_params);
            void finalize();
            std::shared_ptr<GasOilTwoPhaseHystParams> getGasOilParams();
            std::shared_ptr<OilWaterTwoPhaseHystParams> getOilWaterParams();
            std::shared_ptr<GasWaterTwoPhaseHystParams> getGasWaterParams();
            void setConfig();
            void setDrainageParamsOilGas(unsigned elemIdx, unsigned satRegionIdx);
            void setDrainageParamsOilWater(unsigned elemIdx, unsigned satRegionIdx);
            void setDrainageParamsGasWater(unsigned elemIdx, unsigned satRegionIdx);
            void setImbibitionParamsOilGas(unsigned elemIdx, unsigned satRegionIdx);
            void setImbibitionParamsOilWater(unsigned elemIdx, unsigned satRegionIdx);
            void setImbibitionParamsGasWater(unsigned elemIdx, unsigned satRegionIdx);
        private:
            bool hasGasWater_();
            bool hasGasOil_();
            bool hasOilWater_();

            std::tuple<EclEpsScalingPointsInfo<Scalar>, EclEpsScalingPoints<Scalar>> readScaledEpsPoints_(
              const EclEpsGridProperties& epsGridProperties, unsigned elemIdx, EclTwoPhaseSystemType type);
            std::tuple<EclEpsScalingPointsInfo<Scalar>, EclEpsScalingPoints<Scalar>>
              readScaledEpsPointsDrainage_(unsigned elemIdx, EclTwoPhaseSystemType type);
            std::tuple<EclEpsScalingPointsInfo<Scalar>, EclEpsScalingPoints<Scalar>>
              readScaledEpsPointsImbibition_(unsigned elemIdx, EclTwoPhaseSystemType type);
            EclMaterialLawManager<TraitsT>::InitParams& init_params_;
            EclMaterialLawManager<TraitsT>& parent_;
            const EclipseState& eclState_;
            std::shared_ptr<GasOilTwoPhaseHystParams> gasOilParams_;
            std::shared_ptr<OilWaterTwoPhaseHystParams> oilWaterParams_;
            std::shared_ptr<GasWaterTwoPhaseHystParams> gasWaterParams_;
        };

        // This class' implementation is defined in "EclMaterialLawManagerReadEffectiveParams.cpp"
        class ReadEffectiveParams {
        public:
            ReadEffectiveParams(EclMaterialLawManager<TraitsT>::InitParams& init_params);
            void read();
        private:
            std::vector<double> normalizeKrValues_(const double tolcrit, const TableColumn& krValues) const;
            void readGasOilParameters_(GasOilEffectiveParamVector& dest, unsigned satRegionIdx);
            template <class TableType>
            void readGasOilFamily2_(
                GasOilEffectiveTwoPhaseParams& effParams,
                const Scalar Swco,
                const double tolcrit,
                const TableType& sofTable,
                const SgfnTable& sgfnTable,
                const std::string& columnName);
            void readGasOilSgof_(GasOilEffectiveTwoPhaseParams& effParams,
                                const Scalar Swco,
                                const double tolcrit,
                                const SgofTable& sgofTable);

            void readGasOilSlgof_(GasOilEffectiveTwoPhaseParams& effParams,
                                const Scalar Swco,
                                const double tolcrit,
                                const SlgofTable& slgofTable);
            void readGasWaterParameters_(GasWaterEffectiveParamVector& dest, unsigned satRegionIdx);
            void readOilWaterParameters_(OilWaterEffectiveParamVector& dest, unsigned satRegionIdx);

            EclMaterialLawManager<TraitsT>::InitParams& init_params_;
            EclMaterialLawManager<TraitsT>& parent_;
            const EclipseState& eclState_;
        }; // end of "class ReadEffectiveParams"

        EclMaterialLawManager<TraitsT>& parent_;
        const EclipseState& eclState_;
        size_t numCompressedElems_;

        std::unique_ptr<EclEpsGridProperties> epsImbGridProperties_; //imbibition
        std::unique_ptr<EclEpsGridProperties> epsGridProperties_;    // drainage

    };  // end of "class InitParams"

public:
    void initFromState(const EclipseState& eclState);

    void initParamsForElements(const EclipseState& eclState, size_t numCompressedElems);

    /*!
     * \brief Modify the initial condition according to the SWATINIT keyword.
     *
     * The method returns the water saturation which yields a givenn capillary
     * pressure. The reason this method is not folded directly into initFromState() is
     * that the capillary pressure given depends on the particuars of how the simulator
     * calculates its initial condition.
     */
    Scalar applySwatinit(unsigned elemIdx,
                         Scalar pcow,
                         Scalar Sw);

    bool enableEndPointScaling() const
    { return enableEndPointScaling_; }

    bool enableHysteresis() const
    { return hysteresisConfig_->enableHysteresis(); }

    MaterialLawParams& materialLawParams(unsigned elemIdx)
    {
        assert(elemIdx <  materialLawParams_.size());
        return materialLawParams_[elemIdx];
    }

    const MaterialLawParams& materialLawParams(unsigned elemIdx) const
    {
        assert(elemIdx <  materialLawParams_.size());
        return materialLawParams_[elemIdx];
    }

    const MaterialLawParams& materialLawParams(unsigned elemIdx, FaceDir::DirEnum facedir) const
    {
        return materialLawParamsFunc_(elemIdx, facedir);
    }

    MaterialLawParams& materialLawParams(unsigned elemIdx, FaceDir::DirEnum facedir)
    {
        return const_cast<MaterialLawParams&>(materialLawParamsFunc_(elemIdx, facedir));
    }

    /*!
     * \brief Returns a material parameter object for a given element and saturation region.
     *
     * This method changes the saturation table idx in the original material law parameter object.
     * In the context of ECL reservoir simulators, this is required to properly handle
     * wells with its own saturation table idx. In order to reset the saturation table idx
     * in the materialLawparams_ call the method with the cells satRegionIdx
     */
    const MaterialLawParams& connectionMaterialLawParams(unsigned satRegionIdx, unsigned elemIdx) const;

    int satnumRegionIdx(unsigned elemIdx) const
    { return satnumRegionArray_[elemIdx]; }

    int getKrnumSatIdx(unsigned elemIdx, FaceDir::DirEnum facedir) const;

    bool hasDirectionalRelperms() const
    {
        return !krnumXArray_.empty() || !krnumYArray_.empty() || !krnumZArray_.empty();
    }

    bool hasDirectionalImbnum() const {
        if (imbnumXArray_.size() > 0 || imbnumYArray_.size() > 0 || imbnumZArray_.size() > 0) {
            return true;
        }
        return false;
    }

    int imbnumRegionIdx(unsigned elemIdx) const
    { return imbnumRegionArray_[elemIdx]; }

    template <class FluidState>
    void updateHysteresis(const FluidState& fluidState, unsigned elemIdx)
    {
        if (!enableHysteresis())
            return;

        MaterialLaw::updateHysteresis(materialLawParams(elemIdx), fluidState);
        if (hasDirectionalRelperms() || hasDirectionalImbnum()) {
            using Dir = FaceDir::DirEnum;
            constexpr int ndim = 3;
            Dir facedirs[ndim] = {Dir::XPlus, Dir::YPlus, Dir::ZPlus};
            for (int i = 0; i<ndim; i++) {
                MaterialLaw::updateHysteresis(materialLawParams(elemIdx, facedirs[i]), fluidState);
            }
        }
    }

    void oilWaterHysteresisParams(Scalar& pcSwMdc,
                                  Scalar& krnSwMdc,
                                  unsigned elemIdx) const;

    void setOilWaterHysteresisParams(const Scalar& pcSwMdc,
                                     const Scalar& krnSwMdc,
                                     unsigned elemIdx);

    void gasOilHysteresisParams(Scalar& pcSwMdc,
                                Scalar& krnSwMdc,
                                unsigned elemIdx) const;

    void setGasOilHysteresisParams(const Scalar& pcSwMdc,
                                   const Scalar& krnSwMdc,
                                   unsigned elemIdx);

    EclEpsScalingPoints<Scalar>& oilWaterScaledEpsPointsDrainage(unsigned elemIdx);

    const EclEpsScalingPointsInfo<Scalar>& oilWaterScaledEpsInfoDrainage(size_t elemIdx) const
    { return oilWaterScaledEpsInfoDrainage_[elemIdx]; }

    template<class Serializer>
    void serializeOp(Serializer& serializer)
    {
        serializer(enableEndPointScaling_);
        serializer(hysteresisConfig_);
        serializer(oilWaterEclEpsConfig_);
        serializer(unscaledEpsInfo_);
        serializer(oilWaterScaledEpsInfoDrainage_);
        serializer(gasWaterEclEpsConfig_);
        serializer(gasOilUnscaledPointsVector_);
        serializer(oilWaterUnscaledPointsVector_);
        serializer(gasWaterUnscaledPointsVector_);
        serializer(gasOilEffectiveParamVector_);
        serializer(oilWaterEffectiveParamVector_);
        serializer(gasWaterEffectiveParamVector_);
        serializer(threePhaseApproach_);
        serializer(twoPhaseApproach_);
        serializer(materialLawParams_);
        serializer(satnumRegionArray_);
        serializer(krnumXArray_);
        serializer(krnumYArray_);
        serializer(krnumZArray_);
        serializer(imbnumRegionArray_);
        serializer(stoneEtas);
        serializer(hasGas);
        serializer(hasOil);
        serializer(hasWater);
        serializer(gasOilConfig);
        serializer(oilWaterConfig);
        serializer(gasWaterConfig);
    }

    bool operator==(const EclMaterialLawManager<TraitsT>&) const;

private:
    const MaterialLawParams& materialLawParamsFunc_(unsigned elemIdx, FaceDir::DirEnum facedir) const;

    void readGlobalEpsOptions_(const EclipseState& eclState);

    void readGlobalHysteresisOptions_(const EclipseState& state);

    void readGlobalThreePhaseOptions_(const Runspec& runspec);

    bool enableEndPointScaling_;
    std::shared_ptr<EclHysteresisConfig> hysteresisConfig_;

    std::shared_ptr<EclEpsConfig> oilWaterEclEpsConfig_;
    std::vector<EclEpsScalingPointsInfo<Scalar>> unscaledEpsInfo_;
    OilWaterScalingInfoVector oilWaterScaledEpsInfoDrainage_;

    std::shared_ptr<EclEpsConfig> gasWaterEclEpsConfig_;

    GasOilScalingPointsVector gasOilUnscaledPointsVector_;
    OilWaterScalingPointsVector oilWaterUnscaledPointsVector_;
    GasWaterScalingPointsVector gasWaterUnscaledPointsVector_;

    GasOilEffectiveParamVector gasOilEffectiveParamVector_;
    OilWaterEffectiveParamVector oilWaterEffectiveParamVector_;
    GasWaterEffectiveParamVector gasWaterEffectiveParamVector_;

    EclMultiplexerApproach threePhaseApproach_ = EclMultiplexerApproach::Default;
    // this attribute only makes sense for twophase simulations!
    enum EclTwoPhaseApproach twoPhaseApproach_ = EclTwoPhaseApproach::GasOil;

    std::vector<MaterialLawParams> materialLawParams_;
    DirectionalMaterialLawParamsPtr dirMaterialLawParams_;

    std::vector<int> satnumRegionArray_;
    std::vector<int> krnumXArray_;
    std::vector<int> krnumYArray_;
    std::vector<int> krnumZArray_;
    std::vector<int> imbnumXArray_;
    std::vector<int> imbnumYArray_;
    std::vector<int> imbnumZArray_;
    std::vector<int> imbnumRegionArray_;
    std::vector<Scalar> stoneEtas_;

    bool hasGas;
    bool hasOil;
    bool hasWater;

    std::shared_ptr<EclEpsConfig> gasOilConfig_;
    std::shared_ptr<EclEpsConfig> oilWaterConfig_;
    std::shared_ptr<EclEpsConfig> gasWaterConfig_;
};
} // namespace Opm

#endif
