/// \file NavStateIndex
/// \author Andrei Gheata (andrei.gheata@cern.ch)
/// \date 12.03.2014

#ifndef VECGEOM_NAVIGATION_NAVSTATEINDEX_H_
#define VECGEOM_NAVIGATION_NAVSTATEINDEX_H_

#include "VecGeom/base/Global.h"
#include "VecGeom/base/Transformation3D.h"
#include "VecGeom/volumes/PlacedVolume.h"
#include "VecGeom/management/GeoManager.h"

#ifdef VECGEOM_ENABLE_CUDA
#include "VecGeom/management/CudaManager.h"
#endif

#ifdef VECGEOM_ROOT
#include "VecGeom/management/RootGeoManager.h"
#endif

#include <iostream>
#include <string>

class TGeoBranchArray;

namespace vecgeom {
inline namespace VECGEOM_IMPL_NAMESPACE {

/**
 * A class describing a current geometry state based on a single index
 * likely there will be such an object for each particle/track currently treated.
 */
class NavStateIndex {
public:
  using Value_t = unsigned long;

private:
  NavIndex_t fNavInd     = 0;     ///< Navigation state index
  NavIndex_t fLastExited = 0;     ///< Navigation state index of the last exited state
  bool fOnBoundary       = false; ///< flag indicating whether track is on boundary of the "Top()" placed volume

public:
  VECCORE_ATT_HOST_DEVICE
  NavStateIndex(NavIndex_t nav_ind = 0) { fNavInd = nav_ind; }

  VECCORE_ATT_HOST_DEVICE
  ~NavStateIndex() {}

  VECGEOM_FORCE_INLINE
  VECCORE_ATT_HOST_DEVICE
  static unsigned char GetMaxLevel()
  {
#ifdef VECCORE_CUDA_DEVICE_COMPILATION
    return vecgeom::globaldevicegeomdata::GetMaxDepth();
#else
    return (unsigned char)GeoManager::Instance().getMaxDepth();
#endif
  }

  // Static accessors
  VECCORE_ATT_HOST_DEVICE
  static NavStateIndex *MakeInstance(int)
  {
    // MaxLevel is 'zero' based (i.e. maxlevel==0 requires one value)
    return new NavStateIndex();
  }

  VECCORE_ATT_HOST_DEVICE
  static NavStateIndex *MakeCopy(NavStateIndex const &other) { return new NavStateIndex(other); }

  VECCORE_ATT_HOST_DEVICE
  static NavStateIndex *MakeInstanceAt(int, void *addr) { return new (addr) NavStateIndex(); }

  VECCORE_ATT_HOST_DEVICE
  static NavStateIndex *MakeCopy(NavStateIndex const &other, void *addr) { return new (addr) NavStateIndex(other); }

  VECCORE_ATT_HOST_DEVICE
  static void ReleaseInstance(NavStateIndex *state)
  {
    // MaxLevel is 'zero' based (i.e. maxlevel==0 requires one value)
    delete state;
  }

  // returns the size in bytes of a NavStateIndex object with internal
  // path depth maxlevel
  VECCORE_ATT_HOST_DEVICE
  static size_t SizeOfInstance(int)
  {
    // MaxLevel is 'zero' based (i.e. maxlevel==0 requires one value)
    return sizeof(NavStateIndex);
  }

  // returns the size in bytes of a NavStateIndex object with internal
  // path depth maxlevel -- including space needed for padding to next aligned object
  // of same kind
  VECCORE_ATT_HOST_DEVICE
  static size_t SizeOfInstanceAlignAware(int)
  {
    // MaxLevel is 'zero' based (i.e. maxlevel==0 requires one value)
    return sizeof(NavStateIndex);
  }

  VECCORE_ATT_HOST_DEVICE
  int GetObjectSize() const { return (int)sizeof(NavStateIndex); }

  VECCORE_ATT_HOST_DEVICE
  static size_t SizeOf(size_t) { return sizeof(NavStateIndex); }

#ifdef VECGEOM_ROOT
  TGeoBranchArray *ToTGeoBranchArray() const;
  NavStateIndex &operator=(TGeoBranchArray const &rhs);
  /**
   * function return the ROOT TGeoNode object which is equivalent to calling Top()
   * function included for convenience; to make porting Geant-V easier; we should eventually get rid of this function
   */
  VECGEOM_FORCE_INLINE
  TGeoNode const *GetCurrentNode() const { return RootGeoManager::Instance().tgeonode(this->Top()); }
#endif

  VECCORE_ATT_HOST_DEVICE
  void CopyTo(NavStateIndex *other) const { *other = *this; }

  // copies a fixed and predetermined number of bytes
  // might be useful for specialized navigators which know the depth + SizeOf in advance
  // N is number of bytes to be copied and can be obtained by a prior call to constexpr NavStateIndex::SizeOf( ... );
  template <size_t N>
  void CopyToFixedSize(NavStateIndex *other) const
  {
    *other = *this;
  }

  VECCORE_ATT_HOST_DEVICE
  VECGEOM_FORCE_INLINE
  static NavIndex_t const *NavIndAddr(NavIndex_t nav_ind)
  {
#ifdef VECCORE_CUDA_DEVICE_COMPILATION
    // checking here for NVCC_DEVICE since the global variable globaldevicegeomgata::gCompact...
    // is marked __device__ and can only be compiled within device compiler passes
    assert(vecgeom::globaldevicegeomdata::GetNavIndex() != nullptr);
    return &vecgeom::globaldevicegeomdata::GetNavIndex()[nav_ind];
#else
#ifndef VECCORE_CUDA
    assert(vecgeom::GeoManager::gNavIndex != nullptr);
    return &vecgeom::GeoManager::gNavIndex[nav_ind];
#else
    // this is the case when we compile with nvcc for host side
    // (failed previously due to undefined symbol vecgeom::cuda::GeoManager::gCompactPlacedVolBuffer)
    assert(false && "reached unimplement code");
    return nullptr;
#endif
#endif
  }

  VECCORE_ATT_HOST_DEVICE
  VECGEOM_FORCE_INLINE
  static NavIndex_t NavInd(NavIndex_t nav_ind) { return *NavIndAddr(nav_ind); }

  VECCORE_ATT_HOST_DEVICE
  VECGEOM_FORCE_INLINE
  static VPlacedVolume const *ToPlacedVolume(size_t index)
  {
#ifdef VECCORE_CUDA_DEVICE_COMPILATION
    // checking here for NVCC_DEVICE since the global variable globaldevicegeomgata::gCompact...
    // is marked __device__ and can only be compiled within device compiler passes
    assert(vecgeom::globaldevicegeomdata::GetCompactPlacedVolBuffer() != nullptr);
    return &vecgeom::globaldevicegeomdata::GetCompactPlacedVolBuffer()[index];
#else
#ifndef VECCORE_CUDA
    assert(vecgeom::GeoManager::gCompactPlacedVolBuffer == nullptr ||
           vecgeom::GeoManager::gCompactPlacedVolBuffer[index].id() == index);
    return &vecgeom::GeoManager::gCompactPlacedVolBuffer[index];
#else
    // this is the case when we compile with nvcc for host side
    // (failed previously due to undefined symbol vecgeom::cuda::GeoManager::gCompactPlacedVolBuffer)
    assert(false && "reached unimplement code");
    (void)index; // avoid unused parameter warning.
    return nullptr;
#endif
#endif
  }

  VECCORE_ATT_HOST_DEVICE
  VECGEOM_FORCE_INLINE
  static unsigned short GetNdaughtersImpl(NavIndex_t nav_ind)
  {
    constexpr unsigned int kOffsetNd = 2 * sizeof(NavIndex_t) + 2;
    auto content_nd                  = (unsigned short *)((unsigned char *)(NavIndAddr(nav_ind)) + kOffsetNd);
    return *content_nd;
  }

  VECGEOM_FORCE_INLINE
  VECCORE_ATT_HOST_DEVICE
  static unsigned char GetLevelImpl(NavIndex_t nav_ind)
  {
    constexpr unsigned int kOffsetLevel = 2 * sizeof(NavIndex_t);
    auto content_level                  = (unsigned char *)(NavIndAddr(nav_ind)) + kOffsetLevel;
    return *content_level;
  }

  VECCORE_ATT_HOST_DEVICE
  VECGEOM_FORCE_INLINE
  static NavIndex_t GetNavIndexImpl(NavIndex_t nav_ind, int level)
  {
    int up            = GetLevelImpl(nav_ind) - level;
    NavIndex_t mother = nav_ind;
    while (mother && up--)
      mother = NavInd(mother);
    return mother;
  }

  VECGEOM_FORCE_INLINE
  VECCORE_ATT_HOST_DEVICE
  static NavIndex_t PopImpl(NavIndex_t nav_ind) { return (nav_ind > 0) ? NavInd(nav_ind) : 0; }

  VECGEOM_FORCE_INLINE
  VECCORE_ATT_HOST_DEVICE
  static NavIndex_t PushImpl(NavIndex_t nav_ind, VPlacedVolume const *v)
  {
    return (nav_ind > 0) ? NavInd(nav_ind + 3 + v->GetChildId()) : 1;
  }

  VECGEOM_FORCE_INLINE
  VECCORE_ATT_HOST_DEVICE
  static VPlacedVolume const *TopImpl(NavIndex_t nav_ind)
  {
    return (nav_ind > 0) ? ToPlacedVolume(NavInd(nav_ind + 1)) : nullptr;
  }
#if (defined( __SYCL_DEVICE_ONLY__))
  SYCL_EXTERNAL
#else
  VECCORE_ATT_HOST_DEVICE
#endif  
  static void TopMatrixImpl(NavIndex_t nav_ind, Transformation3D &trans);

  VECCORE_ATT_HOST_DEVICE
  static Vector3D<Precision> GlobalToLocalImpl(NavIndex_t nav_ind, Vector3D<Precision> const &globalpoint)
  {
    Transformation3D trans;
    TopMatrixImpl(nav_ind, trans);
    Vector3D<Precision> local = trans.Transform(globalpoint);
    return local;
  }

  // Intrerface methods
  VECGEOM_FORCE_INLINE
  VECCORE_ATT_HOST_DEVICE
  VPlacedVolume const *GetLastExited() const { return TopImpl(fLastExited); }

  VECGEOM_FORCE_INLINE
  VECCORE_ATT_HOST_DEVICE
  void SetLastExited() { fLastExited = fNavInd; }

  VECCORE_ATT_HOST_DEVICE
  VECGEOM_FORCE_INLINE
  unsigned short GetNdaughters() const { return GetNdaughtersImpl(fNavInd); }

  VECGEOM_FORCE_INLINE
  VECCORE_ATT_HOST_DEVICE
  void Push(VPlacedVolume const *v)
  {
    // fLastExited = fNavInd;
    fNavInd = PushImpl(fNavInd, v);
  }

  VECGEOM_FORCE_INLINE
  VECCORE_ATT_HOST_DEVICE
  void Pop()
  {
    // fLastExited = fNavInd;
    fNavInd = PopImpl(fNavInd);
  }

  VECGEOM_FORCE_INLINE
  VECCORE_ATT_HOST_DEVICE
  VPlacedVolume const *Top() const { return TopImpl(fNavInd); }

  /**
   * returns the number of FILLED LEVELS such that
   * state.GetNode( state.GetLevel() ) == state.Top()
   */
  VECGEOM_FORCE_INLINE
  VECCORE_ATT_HOST_DEVICE
  unsigned char GetLevel() const { return GetLevelImpl(fNavInd); }

  /** Compatibility getter for NavigationState interface */
  VECGEOM_FORCE_INLINE
  VECCORE_ATT_HOST_DEVICE
  unsigned char GetCurrentLevel() const { return GetLevel() + 1; }

  /**
   * Returns the navigation index for a level smaller/equal than the current level.
   */
  VECCORE_ATT_HOST_DEVICE
  VECGEOM_FORCE_INLINE
  NavIndex_t GetNavIndex(int level) const { return GetNavIndexImpl(fNavInd, level); }

  /**
   * Returns the placed volume at a evel smaller/equal than the current level.
   */
  VECGEOM_FORCE_INLINE
  VECCORE_ATT_HOST_DEVICE
  VPlacedVolume const *At(int level) const
  {
    auto parent = GetNavIndexImpl(fNavInd, level);
    return (parent > 0) ? ToPlacedVolume(NavInd(parent + 1)) : nullptr;
  }

  /**
   * Returns the index of a placed volume at a evel smaller/equal than the current level.
   */
  VECGEOM_FORCE_INLINE
  VECCORE_ATT_HOST_DEVICE
  size_t ValueAt(int level) const
  {
    auto parent = GetNavIndexImpl(fNavInd, level);
    return (parent > 0) ? (size_t)NavInd(parent + 1) : 0;
  }

  VECCORE_ATT_HOST_DEVICE
  void TopMatrix(Transformation3D &trans) const { TopMatrixImpl(fNavInd, trans); }

  VECCORE_ATT_HOST_DEVICE
  void TopMatrix(int tolevel, Transformation3D &trans) const
  {
    TopMatrixImpl(GetNavIndexImpl(fNavInd, tolevel), trans);
  }

  // returning a "delta" transformation that can transform
  // coordinates given in reference frame of this->Top() to the reference frame of other->Top()
  // simply with otherlocalcoordinate = delta.Transform( thislocalcoordinate )
  VECCORE_ATT_HOST_DEVICE
  void DeltaTransformation(NavStateIndex const &other, Transformation3D &delta) const;

  // VECGEOM_FORCE_INLINE
  VECCORE_ATT_HOST_DEVICE
  Vector3D<Precision> GlobalToLocal(Vector3D<Precision> const &localpoint) const
  {
    return GlobalToLocalImpl(fNavInd, localpoint);
  }

  VECCORE_ATT_HOST_DEVICE
  Vector3D<Precision> GlobalToLocal(Vector3D<Precision> const &localpoint, int tolevel) const
  {
    return GlobalToLocalImpl(GetNavIndexImpl(fNavInd, tolevel), localpoint);
  }

  /**
   * calculates if other navigation state takes a different branch in geometry path or is on same branch
   * ( two states are on same branch if one can connect the states just by going upwards or downwards ( or do nothing ))
   */
  VECCORE_ATT_HOST_DEVICE
  int Distance(NavStateIndex const &) const;

  // returns a string representation of a (relative) sequence of operations/moves
  // that transforms this navigation state into the other navigation state
  // example:
  // state1 = /0/1/1/
  // state2 = /0/2/2/3
  // results in string
  // "/up/horiz/1/down/2/down/3" with 4 operations "up", "horiz", "down", "down"
  // the sequence of moves is the following
  // up: /0/1/1 --> /0/1/
  // horiz/1 : 0/1 --> /0/2 ( == /0/(1+1) )   "we are hopping from daughter 1 to 2 (which corresponds to a step of 1)"
  // down/2 : /0/2 --> /0/2/2   "going further down 2nd daughter"
  // down/3 : /0/2/2/3 --> /0/2/2/3  "going further down 2nd daughter"
  std::string RelativePath(NavStateIndex const & /*other*/) const;

  // functions useful to "serialize" navigationstate
  // the Vector-of-Indices basically describes the path on the tree taken from top to bottom
  // an index corresponds to a daughter

  void GetPathAsListOfIndices(std::list<uint> &indices) const;
  void ResetPathFromListOfIndices(VPlacedVolume const *world, std::list<uint> const &indices);

  // replaces the volume pointers from CPU volumes in fPath
  // to the equivalent pointers on the GPU
  // uses the CudaManager to do so
  void ConvertToGPUPointers() {}

  // replaces the pointers from GPU volumes in fPath
  // to the equivalent pointers on the CPU
  // uses the CudaManager to do so
  void ConvertToCPUPointers() {}

  // clear all information
  VECGEOM_FORCE_INLINE
  VECCORE_ATT_HOST_DEVICE
  void Clear()
  {
    fNavInd     = 0;
    fLastExited = 0;
    fOnBoundary = false;
  }

  VECCORE_ATT_HOST_DEVICE
  void Print() const;

  VECCORE_ATT_HOST_DEVICE
  void Dump() const { Print(); }

  VECGEOM_FORCE_INLINE
  VECCORE_ATT_HOST_DEVICE
  bool HasSamePathAsOther(NavStateIndex const &other) const { return (fNavInd == other.fNavInd); }

  void printValueSequence(std::ostream & = std::cerr) const;

  // calculates a checksum along the path
  // can be used (as a quick criterion) to see whether 2 states are same
  unsigned long getCheckSum() const { return (unsigned long)fNavInd; }

  /**
    function returning whether the point (current navigation state) is outside the detector setup
  */
  VECGEOM_FORCE_INLINE
  VECCORE_ATT_HOST_DEVICE
  bool IsOutside() const { return (fNavInd == 0); }

  VECGEOM_FORCE_INLINE
  VECCORE_ATT_HOST_DEVICE
  bool IsOnBoundary() const { return fOnBoundary; }

  VECGEOM_FORCE_INLINE
  VECCORE_ATT_HOST_DEVICE
  void SetBoundaryState(bool b) { fOnBoundary = b; }
};

/**
 * encodes the geometry path as a concatenated string of ( Value_t ) present in fPath
 */
inline void NavStateIndex::printValueSequence(std::ostream &stream) const
{
  auto level = GetLevel();
  for (int i = 0; i < level + 1; ++i) {
    auto pvol = At(i);
    if (pvol) stream << "/" << ValueAt(i) << "(" << pvol->GetLabel() << ")";
  }
}

} // namespace VECGEOM_IMPL_NAMESPACE
} // namespace vecgeom

#endif // VECGEOM_NAVIGATION_NAVSTATEINDEX_H_
