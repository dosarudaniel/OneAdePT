// SPDX-FileCopyrightText: 2021 CERN
// SPDX-License-Identifier: Apache-2.0

#include <CL/sycl.hpp>
#include <dpct/dpct.hpp>
#include "example9.dp.hpp"
#include <stdlib.h>

#include <AdePT/1/LoopNavigator.h>
#include <CopCore/1/PhysicalConstants.h>

#include <G4HepEmGammaManager.hh>
#include <G4HepEmTrack.hh>
#include <G4HepEmGammaInteractionCompton.hh>
#include <G4HepEmGammaInteractionConversion.hh>

#if (defined( __SYCL_DEVICE_ONLY__))
#define log sycl::log
#define exp sycl::exp
#else
#define exp std::exp
#define log std::log
#endif

// Pull in implementation.
#include <G4HepEmGammaManager.icc>
#include <G4HepEmGammaInteractionCompton.icc>
#include <G4HepEmGammaInteractionConversion.icc>

constexpr double kPush = 1.e-8 * copcore::units::cm;

void TransportGammas(Track *gammas, const adept::MParray *active, Secondaries secondaries,
                     adept::MParray *activeQueue, adept::MParray *relocateQueue, GlobalScoring *scoring,
		     sycl::nd_item<3> item_ct1,
                        struct G4HepEmGammaManager *gammaManager_p,
                        struct G4HepEmParameters *g4HepEmPars_p,
                        struct G4HepEmData *g4HepEmData_p)
{
  /*
  int activeSize = active->size();
  for (int i = item_ct1.get_group(2) * item_ct1.get_local_range().get(2) +
               item_ct1.get_local_id(2);
       i < activeSize;
       i += item_ct1.get_local_range().get(2) * item_ct1.get_group_range(2)) {
    const int slot      = (*active)[i];
    Track &currentTrack = gammas[slot];
    // auto volume         = currentTrack.currentState.Top();
    // if (volume == nullptr) {
    //   // The particle left the world, kill it by not enqueuing into activeQueue.
    //   continue;
    // }

    // Init a track with the needed data to call into G4HepEm.
    G4HepEmTrack emTrack;
    emTrack.SetEKin(currentTrack.energy);
    // For now, just assume a single material.
    int theMCIndex = 1;
    emTrack.SetMCIndex(theMCIndex);

    // Sample the `number-of-interaction-left` and put it into the track.
    for (int ip = 0; ip < 3; ++ip) {
      double numIALeft = currentTrack.numIALeft[ip];
      if (numIALeft <= 0) {
	numIALeft = -log(currentTrack.Uniform());
        currentTrack.numIALeft[ip] = numIALeft;
      }
      emTrack.SetNumIALeft(numIALeft, ip);
    }

    // Call G4HepEm to compute the physics step limit.
    gammaManager_p->HowFar(g4HepEmData_p, g4HepEmPars_p, &emTrack);

    // Get result into variables.
    double geometricalStepLengthFromPhysics = emTrack.GetGStepLength();
    int winnerProcessIndex = emTrack.GetWinnerProcessIndex();
    // Leave the range and MFP inside the G4HepEmTrack. If we split kernels, we
    // also need to carry them over!

    // Check if there's a volume boundary in between.
    double geometryStepLength = 1.0;
        // LoopNavigator::ComputeStepAndNextVolume(currentTrack.pos, currentTrack.dir, geometricalStepLengthFromPhysics,
        //                                         currentTrack.currentState, currentTrack.nextState);
    currentTrack.pos += (geometryStepLength + kPush) * currentTrack.dir;

    if (currentTrack.nextState.IsOnBoundary()) {
      emTrack.SetGStepLength(geometryStepLength);
      emTrack.SetOnBoundary(true);
    }

    gammaManager_p->UpdateNumIALeft(&emTrack);

    // Save the `number-of-interaction-left` in our track.
    for (int ip = 0; ip < 3; ++ip) {
      double numIALeft           = emTrack.GetNumIALeft(ip);
      currentTrack.numIALeft[ip] = numIALeft;
    }

    if (currentTrack.nextState.IsOnBoundary()) {
      // For now, just count that we hit something.
      sycl::atomic<int>(sycl::global_ptr<int>(&scoring->hits)).fetch_add(1);

      activeQueue->push_back(slot);
      //relocateQueue->push_back(slot);

      //LoopNavigator::RelocateToNextVolume(currentTrack.pos, currentTrack.dir, currentTrack.nextState);
      
      // Move to the next boundary.
      currentTrack.SwapStates();
      continue;
    } else if (winnerProcessIndex < 0) {
      // No discrete process, move on.
      activeQueue->push_back(slot);
      continue;
    }

    // Reset number of interaction left for the winner discrete process.
    // (Will be resampled in the next iteration.)
    currentTrack.numIALeft[winnerProcessIndex] = -1.0;

    // Perform the discrete interaction.
    RanluxppDoubleEngine rnge(&currentTrack.rngState);

    const double energy   = currentTrack.energy;

    switch (winnerProcessIndex) {
    case 0: {
      // Invoke gamma conversion to e-/e+ pairs, if the energy is above the threshold.
      if (energy < 2 * copcore::units::kElectronMassC2) {
        activeQueue->push_back(slot);
        continue;
      }

      double logEnergy = log((double)energy);
      double elKinEnergy, posKinEnergy;
      SampleKinEnergies(g4HepEmData_p, energy, logEnergy, theMCIndex, elKinEnergy, posKinEnergy, &rnge);

      double dirPrimary[] = {currentTrack.dir.x(), currentTrack.dir.y(), currentTrack.dir.z()};
      double dirSecondaryEl[3], dirSecondaryPos[3];
      SampleDirections(dirPrimary, dirSecondaryEl, dirSecondaryPos, elKinEnergy, posKinEnergy, &rnge);

      Track &electron = secondaries.electrons.NextTrack();
      Track &positron = secondaries.positrons.NextTrack();
      sycl::atomic<int>(sycl::global_ptr<int>(&scoring->secondaries))
          .fetch_add(2);

      electron.InitAsSecondary(currentTrack);
      electron.energy = elKinEnergy;
      electron.dir.Set(dirSecondaryEl[0], dirSecondaryEl[1], dirSecondaryEl[2]);

      positron.InitAsSecondary(currentTrack);
      positron.energy = posKinEnergy;
      positron.dir.Set(dirSecondaryPos[0], dirSecondaryPos[1], dirSecondaryPos[2]);

      // The current track is killed by not enqueuing into the next activeQueue.
      break;
    }
    case 1: {
      // Invoke Compton scattering of gamma.
      constexpr double LowEnergyThreshold = 100 * copcore::units::eV;
      if (energy < LowEnergyThreshold) {
        activeQueue->push_back(slot);
        continue;
      }
      const double origDirPrimary[] = {currentTrack.dir.x(), currentTrack.dir.y(), currentTrack.dir.z()};
      double dirPrimary[3];
      const double newEnergyGamma = SamplePhotonEnergyAndDirection(energy, dirPrimary, origDirPrimary, &rnge);
      vecgeom::Vector3D<double> newDirGamma(dirPrimary[0], dirPrimary[1], dirPrimary[2]);

      const double energyEl = energy - newEnergyGamma;
      if (energyEl > LowEnergyThreshold) {
        // Create a secondary electron and sample/compute directions.
        Track &electron = secondaries.electrons.NextTrack();
        sycl::atomic<int>(sycl::global_ptr<int>(&scoring->secondaries))
            .fetch_add(1);

        electron.InitAsSecondary(currentTrack);
        electron.energy = energyEl;
        electron.dir = energy * currentTrack.dir - newEnergyGamma * newDirGamma;
        electron.dir.Normalize();
      } else {
        dpct::atomic_fetch_add(&scoring->energyDeposit, energyEl);
      }

      // Check the new gamma energy and deposit if below threshold.
      if (newEnergyGamma > LowEnergyThreshold) {
        currentTrack.energy = newEnergyGamma;
        currentTrack.dir = newDirGamma;

        // The current track continues to live.
        activeQueue->push_back(slot);
      } else {
        dpct::atomic_fetch_add(&scoring->energyDeposit, newEnergyGamma);
        // The current track is killed by not enqueuing into the next activeQueue.
      }
      break;
    }
    case 2: {
      // Invoke photoelectric process: right now only absorb the gamma.
      dpct::atomic_fetch_add(&scoring->energyDeposit, energy);
      // The current track is killed by not enqueuing into the next activeQueue.
      break;
    }
    }
  }
  */
}
