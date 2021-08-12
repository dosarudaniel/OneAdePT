// SPDX-FileCopyrightText: 2020 CERN
// SPDX-License-Identifier: Apache-2.0

// Author: J. Apostolakis  Nov/Dec 2020

#pragma once

#include <VecGeom/base/Vector3D.h>
#include <CopCore/1/PhysicalConstants.h>

#include <AdePT/1/BlockData.h>
#include <AdePT/1/LoopNavigator.h>

#include <Field/1/ConstBzFieldStepper.h>

#if (defined( __SYCL_DEVICE_ONLY__))
#define log sycl::log
#define exp sycl::exp
#define cos sycl::cos
#define sin sycl::sin
#define pow sycl::pow
#define frexp sycl::frexp
#define ldexp sycl::ldexp
#define modf sycl::modf
#define fabs sycl::fabs
#else
#define log std::log
#define exp std::exp
#define cos std::cos
#define sin std::sin
#define pow std::pow
#define frexp std::frexp
#define ldexp std::ldexp
#define modf std::modf
#define fabs std::fabs
#endif

// Data structures for statistics of propagation chords

class fieldPropagatorConstBz {
public:
  fieldPropagatorConstBz(float Bz) { BzValue = Bz; }
  ~fieldPropagatorConstBz() {}

  void stepInField(double kinE, double mass, int charge, double step,
                                       vecgeom::Vector3D<double> &position, vecgeom::Vector3D<double> &direction);

  template <bool Relocate = true>
  double ComputeStepAndPropagatedState(double kinE, double mass, int charge, double physicsStep,
                                                           vecgeom::Vector3D<double> &position,
                                                           vecgeom::Vector3D<double> &direction,
                                                           vecgeom::NavStateIndex const &current_state,
                                                           vecgeom::NavStateIndex &new_state);

private:
  float BzValue;
};

constexpr double kPushField = 1.e-8 * copcore::units::cm;

// -----------------------------------------------------------------------------

void fieldPropagatorConstBz::stepInField(double kinE, double mass, int charge, double step,
                                                             vecgeom::Vector3D<double> &position,
                                                             vecgeom::Vector3D<double> &direction)
{
  if (charge != 0) {
    double momentumMag = sqrt(kinE * (kinE + 2.0 * mass));

    // For now all particles ( e-, e+, gamma ) can be propagated using this
    //   for gammas  charge = 0 works, and ensures that it goes straight.
    ConstBzFieldStepper helixBz(BzValue);

    vecgeom::Vector3D<double> endPosition  = position;
    vecgeom::Vector3D<double> endDirection = direction;
    helixBz.DoStep(position, direction, charge, momentumMag, step, endPosition, endDirection);
    position  = endPosition;
    direction = endDirection;
  } else {
    // Also move gammas - for now ..
    position = position + step * direction;
  }
}

// Determine the step along curved trajectory for charged particles in a field.
//  ( Same name as as navigator method. )
template <bool Relocate>
double fieldPropagatorConstBz::ComputeStepAndPropagatedState(
    double kinE, double mass, int charge, double physicsStep, vecgeom::Vector3D<double> &position,
    vecgeom::Vector3D<double> &direction, vecgeom::NavStateIndex const &current_state,
    vecgeom::NavStateIndex &next_state)
{
  double momentumMag = sqrt(kinE * (kinE + 2.0 * mass));
  double momentumXYMag =
      momentumMag * sqrt((1. - direction[2]) * (1. + direction[2])); // only XY component matters for the curvature

  double curv = fabs(ConstBzFieldStepper::kB2C * charge * BzValue) / (momentumXYMag + 1.0e-30); // norm for step

  constexpr double gEpsilonDeflect = 1.E-2 * copcore::units::cm;

  // acceptable lateral error from field ~ related to delta_chord sagital distance

  // constexpr double invEpsD= 1.0 / gEpsilonDeflect;

  double safeLength =
      sqrt(2 * gEpsilonDeflect / curv); // max length along curve for deflectionn
                                        // = sqrt( 2.0 / ( invEpsD * curv) ); // Candidate for fast inv-sqrt

  ConstBzFieldStepper helixBz(BzValue);

  double stepDone = 0.0;
  double remains  = physicsStep;

  const double epsilon_step = 1.0e-7 * physicsStep; // Ignore remainder if < e_s * PhysicsStep

  if (charge == 0) {
    // if (Relocate) {
    //   stepDone = LoopNavigator::ComputeStepAndPropagatedState(position, direction, remains, current_state, next_state);
    // } else {
    //   stepDone = LoopNavigator::ComputeStepAndNextVolume(position, direction, remains, current_state, next_state);
    // }
    position += (stepDone + kPushField) * direction;
  } else {
    bool fullChord = false;

    //  Locate the intersection of the curved trajectory and the boundaries of the current
    //    volume (including daughters).
    //  Most electron tracks are short, limited by physics interactions -- the expected
    //    average value of iterations is small.
    //    ( Measuring iterations to confirm the maximum. )
    constexpr int maxChordIters = 10;
    int chordIters              = 0;
    do {
      vecgeom::Vector3D<double> endPosition  = position;
      vecgeom::Vector3D<double> endDirection = direction;
      double safeMove                        = std::min(remains, safeLength);

      helixBz.DoStep(position, direction, charge, momentumMag, safeMove, endPosition, endDirection);

      vecgeom::Vector3D<double> chordVec = endPosition - position;
      double chordLen                    = chordVec.Length();
      vecgeom::Vector3D<double> chordDir = (1.0 / chordLen) * chordVec;

      double move;
      //      if (Relocate) {
      //        move = LoopNavigator::ComputeStepAndPropagatedState(position, chordDir, chordLen, current_state, next_state);
      //     } else {
      //  move = LoopNavigator::ComputeStepAndNextVolume(position, chordDir, chordLen, current_state, next_state);
      //  }

      fullChord = (move == chordLen);
      if (fullChord) {
        position  = endPosition;
        direction = endDirection;
        move      = safeMove;
      } else {
        // Accept the intersection point on the surface.  This means that
        //   the point at the boundary will be on the 'straight'-line chord,
        //   not the curved trajectory.
        // ( This involves a bias -- relevant for muons in trackers.
        //   Currently it's controlled/limited by the acceptable step size ie. 'safeLength' )
        position = position + move * chordDir;

        // Primitive approximation of end direction ...
        double fraction = chordLen > 0 ? move / chordLen : 0.0;
        direction       = direction * (1.0 - fraction) + endDirection * fraction;
        direction       = direction.Unit();
      }
      stepDone += move;
      remains -= move;
      chordIters++;

    } while ((!next_state.IsOnBoundary()) && fullChord && (remains > epsilon_step) && (chordIters < maxChordIters));
  }

  return stepDone;
}
