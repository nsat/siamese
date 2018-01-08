/** \file
    \brief Siamese FEC Implementation: Tools
    \copyright Copyright (c) 2017 Christopher A. Taylor.  All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    * Neither the name of Siamese nor the names of its contributors may be
      used to endorse or promote products derived from this software without
      specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

/**
    Tools:

    + System headers
    + Debug breakpoints/asserts
    + Compiler-specific code wrappers
    + PCGRandom implementation
    + Microsecond timing
    + Windowed minimum/maximum
*/

#include <stdint.h> // uint32_t
#include <string.h> // memcpy
#include <new> // std::nothrow


//------------------------------------------------------------------------------
// Portability macros

// Compiler-specific debug break
#if defined(_DEBUG) || defined(DEBUG)
    #define SIAMESE_DEBUG
    #ifdef _WIN32
        #define SIAMESE_DEBUG_BREAK() __debugbreak()
    #else
        #define SIAMESE_DEBUG_BREAK() __builtin_trap()
    #endif
    #define SIAMESE_DEBUG_ASSERT(cond) { if (!(cond)) { SIAMESE_DEBUG_BREAK(); } }
#else
    #define SIAMESE_DEBUG_BREAK() do {} while (false);
    #define SIAMESE_DEBUG_ASSERT(cond) do {} while (false);
#endif

// Compiler-specific force inline keyword
#ifdef _MSC_VER
    #define SIAMESE_FORCE_INLINE inline __forceinline
#else
    #define SIAMESE_FORCE_INLINE inline __attribute__((always_inline))
#endif


namespace siamese {


//------------------------------------------------------------------------------
// PCG PRNG

/// From http://www.pcg-random.org/
class PCGRandom
{
public:
    void Seed(uint64_t y, uint64_t x = 0)
    {
        State = 0;
        Inc = (y << 1u) | 1u;
        Next();
        State += x;
        Next();
    }

    uint32_t Next()
    {
        const uint64_t oldstate = State;
        State = oldstate * UINT64_C(6364136223846793005) + Inc;
        const uint32_t xorshifted = (uint32_t)(((oldstate >> 18) ^ oldstate) >> 27);
        const uint32_t rot = oldstate >> 59;
        return (xorshifted >> rot) | (xorshifted << ((uint32_t)(-(int32_t)rot) & 31));
    }

    uint64_t State = 0, Inc = 0;
};


//------------------------------------------------------------------------------
// Timing

/// Platform independent high-resolution timers
uint64_t GetTimeUsec();
uint64_t GetTimeMsec();


//------------------------------------------------------------------------------
// LightVector

/**
    Super light-weight replacement for std::vector class

    Features:
    + Tuned for Siamese allocation needs.
    + Never shrinks memory usage.
    + Minimal well-defined API: Only functions used several times.
    + Preallocates some elements to improve speed of short runs.
    + Uses normal allocator.
    + Growing the vector does not initialize the new elements for speed.
    + Does not throw on out-of-memory error.
*/
template<typename T>
class LightVector
{
    /// Number of preallocated elements
    static const unsigned kPreallocated = 25; // Tuned for Siamese
    T PreallocatedData[kPreallocated];

    /// Size of vector: Count of elements in the vector
    unsigned Size = 0;

    /// Vector data
    T* DataPtr = nullptr;

    /// Number of elements allocated
    unsigned Allocated = kPreallocated;

public:
    /// Resize the vector to the given number of elements.
    /// After this call, all elements are Uninitialized.
    /// If new size is greater than preallocated size,
    /// it will allocate a new buffer that is 1.5x larger.
    /// Returns false if memory could not be allocated
    bool SetSize_NoCopy(unsigned elements)
    {
        SIAMESE_DEBUG_ASSERT(Size <= Allocated);

        // If it is actually expanding, and it needs to grow:
        if (elements > Allocated)
        {
            const unsigned newAllocated = (elements * 3) / 2;
            T* newData = new(std::nothrow) T[newAllocated];
            if (!newData)
                return false;
            T* oldData = DataPtr;
            Allocated  = newAllocated;
            DataPtr    = newData;

            // Delete old data without copying
            if (oldData != &PreallocatedData[0])
                delete[] oldData;
        }

        Size = elements;
        return true;
    }

    /// Resize the vector to the given number of elements.
    /// If new size is smaller than current size, it will truncate.
    /// If new size is greater than current size, new elements will be Uninitialized.
    /// And if new size is greater than preallocated size, it will allocate a new
    /// buffer that is 1.5x larger and that will keep the existing data,
    /// leaving any new elements Uninitialized.
    /// Returns false if memory could not be allocated
    bool SetSize_Copy(unsigned elements)
    {
        SIAMESE_DEBUG_ASSERT(Size <= Allocated);

        // If it is actually expanding, and it needs to grow:
        if (elements > Allocated)
        {
            const unsigned newAllocated = (elements * 3) / 2;
            T* newData = new(std::nothrow) T[newAllocated];
            if (!newData)
                return false;
            T* oldData = DataPtr;
            Allocated  = newAllocated;
            DataPtr    = newData;

            // Copy data before deletion
            memcpy(newData, oldData, sizeof(T) * Size);

            if (oldData != &PreallocatedData[0])
                delete[] oldData;
        }

        Size = elements;
        return true;
    }

    /// Expand as needed and add one element to the end.
    /// Returns false if memory could not be allocated
    SIAMESE_FORCE_INLINE bool Append(const T& rhs)
    {
        const unsigned newSize = Size + 1;
        if (!SetSize_Copy(newSize))
            return false;
        DataPtr[newSize - 1] = rhs;
        return true;
    }

    /// Set size to zero
    SIAMESE_FORCE_INLINE void Clear()
    {
        Size = 0;
    }

    /// Get current size (initially 0)
    SIAMESE_FORCE_INLINE unsigned GetSize() const
    {
        return Size;
    }

    /// Return a reference to an element
    SIAMESE_FORCE_INLINE T& GetRef(int index) const
    {
        return DataPtr[index];
    }

    /// Return a pointer to an element
    SIAMESE_FORCE_INLINE T* GetPtr(int index) const
    {
        return DataPtr + index;
    }

    /// Initialize preallocated data
    SIAMESE_FORCE_INLINE LightVector()
    {
        DataPtr = &PreallocatedData[0];
    }
};


//------------------------------------------------------------------------------
// WindowedMinMax

template<typename T> struct WindowedMinCompare
{
    SIAMESE_FORCE_INLINE bool operator()(const T x, const T y) const
    {
        return x <= y;
    }
};

template<typename T> struct WindowedMaxCompare
{
    SIAMESE_FORCE_INLINE bool operator()(const T x, const T y) const
    {
        return x >= y;
    }
};

/// Templated class that calculates a running windowed minimum or maximum with
/// a fixed time and resource cost.
template<typename T, class CompareT> class WindowedMinMax
{
public:
    typedef uint64_t TimeT;
    CompareT Compare;

    struct Sample
    {
        /// Sample value
        T Value;

        /// Timestamp of data collection
        TimeT Timestamp;


        /// Default values and initializing constructor
        explicit Sample(T value = 0, TimeT timestamp = 0)
            : Value(value)
            , Timestamp(timestamp)
        {
        }

        /// Check if a timeout expired
        inline bool TimeoutExpired(TimeT now, TimeT timeout)
        {
            return (TimeT)(now - Timestamp) > timeout;
        }
    };


    static const unsigned kSampleCount = 3;

    Sample Samples[kSampleCount];


    bool IsValid() const
    {
        return Samples[0].Value != 0; ///< ish
    }

    T GetBest() const
    {
        return Samples[0].Value;
    }

    void Reset(const Sample sample = Sample())
    {
        Samples[0] = Samples[1] = Samples[2] = sample;
    }

    void Update(T value, TimeT timestamp, const TimeT windowLengthTime)
    {
        const Sample sample(value, timestamp);

        // On the first sample, new best sample, or if window length has expired:
        if (!IsValid() ||
            Compare(value, Samples[0].Value) ||
            Samples[2].TimeoutExpired(sample.Timestamp, windowLengthTime))
        {
            Reset(sample);
            return;
        }

        // Insert the new value into the sorted array
        if (Compare(value, Samples[1].Value))
            Samples[2] = Samples[1] = sample;
        else if (Compare(value, Samples[2].Value))
            Samples[2] = sample;

        // Expire best if it has been the best for a long time
        if (Samples[0].TimeoutExpired(sample.Timestamp, windowLengthTime))
        {
            // Also expire the next best if needed
            if (Samples[1].TimeoutExpired(sample.Timestamp, windowLengthTime))
            {
                Samples[0] = Samples[2];
                Samples[1] = sample;
            }
            else
            {
                Samples[0] = Samples[1];
                Samples[1] = Samples[2];
            }
            Samples[2] = sample;
            return;
        }

        // Quarter of window has gone by without a better value - Use the second-best
        if (Samples[1].Value == Samples[0].Value &&
            Samples[1].TimeoutExpired(sample.Timestamp, windowLengthTime / 4))
        {
            Samples[2] = Samples[1] = sample;
            return;
        }

        // Half the window has gone by without a better value - Use the third-best one
        if (Samples[2].Value == Samples[1].Value &&
            Samples[2].TimeoutExpired(sample.Timestamp, windowLengthTime / 2))
        {
            Samples[2] = sample;
        }
    }
};


} // namespace siamese
