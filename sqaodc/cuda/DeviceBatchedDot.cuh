#pragma once

#include <cuda/DeviceMatrix.h>
#include <cuda/DeviceSegmentedSum.cuh>

namespace sqaod_cuda {

template<class Vout, class Vin0, class Vin1>
struct In2TypeDotPtr {
    typedef In2TypeDotPtr<Vout, Vin0, Vin1> SelfType;

    __host__ __device__
    In2TypeDotPtr(const Vin0 *_d_x, const Vin1 *_d_y) : d_x(_d_x), d_y(_d_y) { }
    __device__ __forceinline__
    Vout operator[](sq::IdxType idx) const {
        return (Vout)d_x[idx] * (Vout)d_y[idx];
    }

    __device__ __forceinline__
    Vout operator[](const int2 &idx2) const {
        return (Vout)d_x[idx2.x] * (Vout)d_y[idx2.y];
    }

    __device__ __forceinline__
    SelfType operator+(sq::IdxType idx) const {
        return SelfType(&d_x[idx], &d_y[idx]);
    }

    const Vin0 *d_x;
    const Vin1 *d_y;
};


template<class real>
using InDotPtr = In2TypeDotPtr<real, real, real>;

/*
 * OffsetIterator
 */

struct Offset2way  {
    __host__
    Offset2way(const sq::IdxType *_d_offset, sq::SizeType _stride0, sq::SizeType _stride1)
            : d_offset(_d_offset), stride0(_stride0), stride1(_stride1) { }

    __device__ __forceinline__
    int2 operator[](sq::IdxType idx) const {
        return make_int2(idx * stride0, d_offset[idx] * stride1);
    }
    const sq::IdxType *d_offset;
    sq::SizeType stride0, stride1;
};

__device__ __forceinline__
int2 operator+(const int2 &lhs, const int v) {
    return make_int2(lhs.x + v, lhs.y + v);
}


/* wrapped offset */

struct WrappedOffset  {
    __host__
    WrappedOffset(int _m, int _stride)
            : m(_m), stride(_stride) { }

    __device__ __forceinline__
    int2 operator[](sq::IdxType idx) const {
        int idxNeighbour = (idx + 1) % m;
        return make_int2(idx * stride, idxNeighbour * stride);
    }
    sq::SizeType m;
    sq::SizeType stride;
};


}


/*
 * iterater traits
 */

namespace std {

template<class Vout, class Vin0, class Vin1>
struct iterator_traits<sqaod_cuda::In2TypeDotPtr<Vout, Vin0, Vin1>> : sqaod_cuda::base_iterator_traits<Vout> { };

template<>
struct iterator_traits<sqaod_cuda::Offset2way> : sqaod_cuda::base_iterator_traits<int2> { };

template<>
struct iterator_traits<sqaod_cuda::WrappedOffset> : sqaod_cuda::base_iterator_traits<int2> { };

}


/* specialized classes */


namespace sqaod_cuda {

/* Scalar version */


template<class V, class OutIt>
struct DeviceBatchedDot : DeviceSegmentedSumType<V, InDotPtr<V>, OutIt, Linear, 1> {
    typedef DeviceSegmentedSumType<V, InDotPtr<V>, OutIt, Linear, 1> Base;
    DeviceBatchedDot(Device &device, DeviceStream *devStream)
            : Base(device, devStream) { }

    DeviceBatchedDot(DeviceStream *devStream) : Base(devStream) { }
    
    void operator()(const DeviceMatrixType<V> &d_x, const DeviceMatrixType<V> &d_y, OutIt outIt) {
        InDotPtr<V> in(d_x.d_data, d_y.d_data);
        throwErrorIf(d_x.stride != d_y.stride, "Different stride to execute batched dot.");
        Base::operator()(in, outIt, Linear(d_x.stride));
    }
};



template<class V, class OutIt>
struct DeviceDotJq : DeviceSegmentedSumType<V, In2TypeDotPtr<V, char, V>, V*, Offset2way, 1> {
    typedef DeviceSegmentedSumType<V, In2TypeDotPtr<V, char, V>, V*, Offset2way, 1> Base;
public:
    
    DeviceDotJq(Device &device, DeviceStream *devStream)
            : Base(device, devStream) {
    }

    DeviceDotJq(DeviceStream *devStream) : Base(devStream) { }
    
    void operator()(const DeviceMatrixType<V> &d_J, const DeviceBitMatrix &d_q,
                    const int *d_yOffset, OutIt outIt) {
        In2TypeDotPtr<V, char, V> in(d_q.d_data, d_J.d_data);
        Offset2way offset(d_yOffset, d_q.stride, d_J.stride);
        Base::operator()(in, outIt, offset);
    }
};



template<class Vout, class Vin>
struct DeviceDotSpins : DeviceSegmentedSumType<Vout, In2TypeDotPtr<Vout, Vin, Vin>, Vout*, WrappedOffset, 1> {
    typedef DeviceSegmentedSumType<Vout, In2TypeDotPtr<Vout, Vin, Vin>, Vout*, WrappedOffset, 1> Base;
public:
    
    DeviceDotSpins(Device &device, DeviceStream *devStream)
            : Base(device, devStream) {
    }

    DeviceDotSpins(DeviceStream *devStream) : Base(devStream) { }
    
    void operator()(const DeviceMatrixType<Vin> &d_q, DeviceVectorType<Vout> *out) {
        In2TypeDotPtr<Vout, Vin, Vin> in(d_q.d_data, d_q.d_data);
        WrappedOffset offset(d_q.rows, d_q.stride);
        Base::operator()(in, out->d_data, offset);
    }
};



/* Vectorized */

/* Value traits class for vector types. */

template<class V> struct ValueTraits;
template<> struct ValueTraits<float> { typedef float4 VectorType; typedef float ScalarType; };
template<> struct ValueTraits<double> { typedef double4 VectorType; typedef double ScalarType; };
template<> struct ValueTraits<char> { typedef char4 VectorType; typedef char ScalarType; };

template<class V>
struct InSumPtrVec4 {
    typedef InSumPtrVec4<V> SelfType;
    typedef typename ValueTraits<V>::VectorType VecIn;
    __host__ __device__
    InSumPtrVec4(const V *_d_x) : d_x((const VecIn*)_d_x) { }

    __device__ __forceinline__
    V operator[](sq::IdxType idx) const {
        VecIn x = d_x[idx];
        return (x.x + x.y ) + (x.z + x.w);
    }

    __device__ __forceinline__
    SelfType operator+(sq::IdxType idx) const {
        return SelfType(&d_x[idx]);
    }

    const VecIn *d_x;
};

template<class V, class OutIt>
struct DeviceBatchedSumVec4 :
            DeviceSegmentedSumType<V, InSumPtrVec4<V>, OutIt, Linear, 4> {
    typedef DeviceSegmentedSumType<V, InSumPtrVec4<V>, OutIt, Linear, 4> Base;
    
    DeviceBatchedSumVec4(Device &device, DeviceStream *devStream = NULL)
            : Base(device, devStream) { }

    DeviceBatchedSumVec4(DeviceStream *devStream) : Base(devStream) { }
    
    void operator()(const DeviceMatrixType<V> &d_mat, OutIt out) {
        InSumPtrVec4<V> in(d_mat.d_data);
        Base::operator()(in, out, Linear(d_mat.stride / 4));
    }
};


template<class Vout, class Vin0, class Vin1>
struct In2TypeDotPtrVec4 {
    typedef In2TypeDotPtrVec4<Vout, Vin0, Vin1> SelfType;
    typedef typename ValueTraits<Vin0>::VectorType VecIn0;
    typedef typename ValueTraits<Vin1>::VectorType VecIn1;
    
    __host__ __device__
    In2TypeDotPtrVec4(const Vin0 *_d_x, const Vin1 *_d_y)
            : d_x((const VecIn0*)_d_x), d_y((const VecIn1*)_d_y) { }
    __device__ __forceinline__
    Vout operator[](sq::IdxType idx) const {
        VecIn0 x = d_x[idx];
        VecIn1 y = d_y[idx];
        return ((Vout)x.x * (Vout)y.x + (Vout)x.y * (Vout)y.y)
                + ((Vout)x.z * (Vout)y.z + (Vout)x.w * (Vout)y.w);
    }
    
    __device__ __forceinline__
    Vout operator[](const int2 &idx2) const {
        VecIn0 x = d_x[idx2.x];
        VecIn1 y = d_y[idx2.y];
        return ((Vout)x.x * (Vout)y.x + (Vout)x.y * (Vout)y.y)
                + ((Vout)x.z * (Vout)y.z + (Vout)x.w * (Vout)y.w);
    }

    __device__ __forceinline__
    SelfType operator+(sq::IdxType idx) const {
        return SelfType(&d_x[idx], &d_y[idx]);
    }

    const VecIn0 *d_x;
    const VecIn1 *d_y;
};


template<class V, class OutIt>
struct DeviceDotJqVec4 :
            DeviceSegmentedSumType<V, In2TypeDotPtrVec4<V, char, V>, OutIt, Offset2way, 4> {
    typedef DeviceSegmentedSumType<V, In2TypeDotPtrVec4<V, char, V>, V*, Offset2way, 4> Base;
    
    DeviceDotJqVec4(Device &device, DeviceStream *devStream = NULL)
            : Base(device, devStream) { }

    DeviceDotJqVec4(DeviceStream *devStream) : Base(devStream) { }
    
    void operator()(const DeviceMatrixType<V> &d_J, const DeviceBitMatrix &d_q,
                    const int *d_yOffset, OutIt out) {
        In2TypeDotPtrVec4<V, char, V> in(d_q.d_data, d_J.d_data);
        Offset2way offset(d_yOffset, d_q.stride / 4, d_J.stride / 4);
        Base::operator()(in, out, offset);
    }
};

}
