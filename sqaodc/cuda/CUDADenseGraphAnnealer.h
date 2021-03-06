#pragma once

#include <sqaodc/common/Common.h>
#include <sqaodc/cuda/Device.h>
#include <sqaodc/cuda/DeviceMatrix.h>
#include <sqaodc/cuda/DeviceArray.h>
#include <sqaodc/cuda/DeviceDenseGraphBatchSearch.h>
#include <sqaodc/cuda/DeviceRandom.h>
#include <sqaodc/cuda/DeviceRandomBuffer.h>


namespace sqaod_cuda {

namespace sq = sqaod;

template<class real>
class CUDADenseGraphAnnealer : public sqaod::cuda::DenseGraphAnnealer<real> {
    typedef sq::MatrixType<real> HostMatrix;
    typedef sq::VectorType<real> HostVector;
    typedef sq::BitSet BitSet;
    typedef sq::BitSetArray BitSetArray;
    typedef sq::SizeType SizeType;
    typedef DeviceMatrixType<real> DeviceMatrix;
    typedef DeviceVectorType<real> DeviceVector;
    typedef DeviceScalarType<real> DeviceScalar;
    typedef DeviceDenseGraphFormulas<real> DeviceFormulas;
    
public:
    CUDADenseGraphAnnealer();

    CUDADenseGraphAnnealer(Device &device);

    ~CUDADenseGraphAnnealer();

    void deallocate();

    void assignDevice(sqaod::cuda::Device &device);

    void assignDevice(Device &device);

    virtual sq::Algorithm selectAlgorithm(sq::Algorithm algo);
    
    void seed(unsigned long long seed);

    void setQUBO(const HostMatrix &W, sq::OptimizeMethod om = sq::optMinimize);

    void setHamiltonian(const HostVector &h, const HostMatrix &J, real c = real(0.));

    /* void getProblemSize(sq::SizeType *N) const; */

    sq::Preferences getPreferences() const;

    /* void setPreference(const Preference &pref); */
    
    const HostVector &get_E() const;

    const BitSetArray &get_x() const;

    void set_q(const BitSet &q);

    void set_qset(const BitSetArray &q);

    const sq::BitSetArray &get_q() const;

    void getHamiltonian(HostVector *h, HostMatrix *J, real *c) const;

    void randomizeSpin();

    void prepare();

    void calculate_E();

    void makeSolution();

    real getSystemE(real G, real beta) const;

    void annealOneStep(real G, real beta) {
        (this->*annealMethod_)(G, beta);
    }

    void annealOneStepSQA(real G, real beta);

    void annealOneStepSA(real kT, real beta);

    /* CUDA Kernels */
    void annealOneStepSQA(DeviceBitMatrix *d_matq, const DeviceVector &d_Jq,
                          const int *d_x, const real *d_random,
                          const DeviceVector &d_h, const DeviceMatrix &d_J, real G, real beta);
    
    void annealOneStepSA(DeviceBitMatrix *d_matq, const DeviceVector &d_Jq,
                         const int *d_x, const real *d_random,
                         const DeviceVector &d_h, const DeviceMatrix &d_J, real Tnorm);

    void calculate_Jq(DeviceVector *d_Jq, const DeviceMatrix &d_J, const DeviceBitMatrix &d_matq,
                      const int *d_flipPos);
private:
    void deallocateProblem();
    void deallocateInternalObjects();

    enum {
        /* FIXME: parameterise */
        maxRandBufCapacity = 128 << 20,
        maxNRunsPerRandGen = 10,
    };

    void syncBits();

    typedef void (CUDADenseGraphAnnealer<real>::*AnnealMethod)(real G, real beta);
    AnnealMethod annealMethod_;
    
    DeviceRandom d_random_;
    DeviceMatrix d_J_;
    DeviceVector d_h_;
    DeviceScalar d_c_;
    DeviceBitMatrix d_matq_;
    DeviceVector d_Jq_;
    DeviceVector h_E_;
    DeviceBitMatrix h_q_;
    sq::SizeType nRunsPerRandGen_;
    DeviceRandomBuffer flipPosBuffer_;
    DeviceRandomBuffer realNumBuffer_;
    HostVector E_;
    uint2 *d_reachCount_;

    sq::BitSetArray xlist_;
    sq::BitSetArray qlist_;

    sq::NullBase *dotJq_;
    sq::NullBase *dotSpins_;
    DeviceScalar h_spinDotSum_;
    
    DeviceStream *devStream_;
    DeviceFormulas devFormulas_;
    DeviceCopy devCopy_;
    DeviceObjectAllocator *devAlloc_;

    typedef CUDADenseGraphAnnealer<real> This;
    typedef sq::DenseGraphAnnealer<real> Base;
    using Base::selectDefaultAlgorithm;
    using Base::selectDefaultSAAlgorithm;
    using Base::N_;
    using Base::m_;
    using Base::om_;
    using Base::algo_;
    /* annealer state */
    using Base::solRandSeedGiven;
    using Base::solPrepared;
    using Base::solProblemSet;
    using Base::solQSet;
    using Base::solEAvailable;
    using Base::solSolutionAvailable;
    using Base::setState;
    using Base::clearState;
    using Base::isRandSeedGiven;
    using Base::isEAvailable;
    using Base::isSolutionAvailable;
    using Base::throwErrorIfProblemNotSet;
    using Base::throwErrorIfNotPrepared;
    using Base::throwErrorIfQNotSet;
};

}
