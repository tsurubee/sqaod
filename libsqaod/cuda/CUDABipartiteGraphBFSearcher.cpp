#include "CUDABipartiteGraphBFSearcher.h"
#include <cmath>
#include <float.h>
#include <algorithm>
#include <exception>

using namespace sqaod_cuda;

template<class real>
CUDABipartiteGraphBFSearcher<real>::CUDABipartiteGraphBFSearcher() {
    tileSize0_ = 1024;
    tileSize1_ = 1024;
    deviceAssigned_ = false;
}

template<class real>
CUDABipartiteGraphBFSearcher<real>::CUDABipartiteGraphBFSearcher(Device &device) {
    tileSize0_ = 1024;
    tileSize1_ = 1024;
    deviceAssigned_ = false;
    assignDevice(device);
}

template<class real>
CUDABipartiteGraphBFSearcher<real>::~CUDABipartiteGraphBFSearcher() {
    deallocate();
}

template<class real>
void CUDABipartiteGraphBFSearcher<real>::deallocate() {
    if (h_packedMinXPairs_.d_data != NULL)
        HostObjectAllocator().deallocate(h_packedMinXPairs_);
    batchSearch_.deallocate();
}

template<class real>
void CUDABipartiteGraphBFSearcher<real>::assignDevice(Device &device) {
    throwErrorIf(deviceAssigned_, "Device already assigned.");
    batchSearch_.assignDevice(device, device.defaultStream());
    deviceAssigned_ = true;
}

template<class real>
void CUDABipartiteGraphBFSearcher<real>::setProblem(const HostVector &b0, const HostVector &b1,
                                                    const HostMatrix &W, sq::OptimizeMethod om) {
    throwErrorIf(!deviceAssigned_, "Device not set.");

    N0_ = b0.size;
    N1_ = b1.size;
    throwErrorIf(63 < N0_, "N0 must be smaller than 64, N0=%d.", N0_);
    throwErrorIf(63 < N1_, "N1 must be smaller than 64, N1=%d.", N1_);
    b0_ = b0;
    b1_ = b1;
    W_ = W;
    om_ = om;
    if (om_ == sq::optMaximize) {
        W_ *= real(-1.);
        b0_ *= real(-1.);
        b1_ *= real(-1.);
    }

    setState(solProblemSet);
}

template<class real>
const sq::BitsPairArray &CUDABipartiteGraphBFSearcher<real>::get_x() const {
    throwErrorIfSolutionNotAvailable();
    return minXPairs_;
}

template<class real>
const sq::VectorType<real> &CUDABipartiteGraphBFSearcher<real>::get_E() const {
    throwErrorIfSolutionNotAvailable();
    return E_;
}

template<class real>
void CUDABipartiteGraphBFSearcher<real>::initSearch() {
    throwErrorIfProblemNotSet();
    if (isInitialized())
        deallocate();

    Emin_ = std::numeric_limits<real>::max();
    x0max_ = 1ull << N0_;
    x1max_ = 1ull << N1_;
    if (x0max_ < tileSize0_) {
        tileSize0_ = (sq::SizeType)x0max_;
        sq::log("Tile size 0 is adjusted to %d for N0=%d", tileSize0_, N0_);
    }
    if (x1max_ < tileSize1_) {
        tileSize1_ = (sq::SizeType)x1max_;
        sq::log("Tile size 1 is adjusted to %d for N1=%d", tileSize1_, N1_);
    }
    batchSearch_.setProblem(b0_, b1_, W_, tileSize0_, tileSize1_);
    SizeType minXPairsSize = (tileSize0_ * tileSize1_) * 2;
    HostObjectAllocator halloc;
    halloc.allocate(&h_packedMinXPairs_, minXPairsSize);

    setState(solInitialized);
}

template<class real>
void CUDABipartiteGraphBFSearcher<real>::finSearch() {
    throwErrorIfNotInitialized();
    batchSearch_.synchronize();
    const DevicePackedBitsPairArray &dPackedXminPairs = batchSearch_.get_minXPairs();
    SizeType nXMin = dPackedXminPairs.size;
    devCopy_(&h_packedMinXPairs_, dPackedXminPairs);
    devCopy_.synchronize();
    
    minXPairs_.clear();
    E_.resize(nXMin);
    E_ = Emin_;
    if (om_ == sq::optMaximize)
        E_ *= real(-1.);
    for (sq::IdxType idx = 0; idx < (sq::IdxType)nXMin; ++idx) {
        sq::Bits bits0, bits1;
        unpackBits(&bits0, h_packedMinXPairs_[idx].bits0, N0_);
        unpackBits(&bits1, h_packedMinXPairs_[idx].bits1, N1_);
        minXPairs_.pushBack(BitsPairArray::ValueType(bits0, bits1)); // FIXME: apply move
    }

    setState(solSolutionAvailable);
}

template<class real>
void CUDABipartiteGraphBFSearcher<real>::searchRange(PackedBits xBegin0, PackedBits xEnd0,
                                                     PackedBits xBegin1, PackedBits xEnd1) {
    throwErrorIfNotInitialized();
    
    /* FIXME: Use multiple searchers, multi GPU */
    throwErrorIf(xBegin0 > xEnd0, "xBegin0 should be larger than xEnd0");
    throwErrorIf(xBegin1 > xEnd1, "xBegin1 should be larger than xEnd1");
    if ((xBegin0 == xEnd0) || (xBegin1 == xEnd1))
        return; /* Nothing to do */
    xBegin0 = std::min(std::max(0ULL, xBegin0), x0max_);
    xEnd0 = std::min(std::max(0ULL, xEnd0), x0max_);
    xBegin1 = std::min(std::max(0ULL, xBegin1), x1max_);
    xEnd1 = std::min(std::max(0ULL, xEnd1), x1max_);
    
    batchSearch_.calculate_E(xBegin0, xEnd0, xBegin1, xEnd1);
    batchSearch_.synchronize();

    real newEmin = batchSearch_.get_Emin();
    if (newEmin < Emin_) {
        batchSearch_.partition_minXPairs(false);
        Emin_ = newEmin;
    }
    else if (newEmin == Emin_) {
        batchSearch_.partition_minXPairs(true);
    }
    /* FIXME: add max limits of # min vectors. */
}

template class sqaod_cuda::CUDABipartiteGraphBFSearcher<float>;
template class sqaod_cuda::CUDABipartiteGraphBFSearcher<double>;
