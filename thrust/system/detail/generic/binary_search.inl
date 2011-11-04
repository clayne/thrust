/*
 *  Copyright 2008-2011 NVIDIA Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */


/*! \file binary_search.inl
 *  \brief Inline file for binary_search.h
 */

#pragma once

#include <thrust/detail/config.h>
#include <thrust/distance.h>
#include <thrust/functional.h>
#include <thrust/binary_search.h>
#include <thrust/iterator/zip_iterator.h>
#include <thrust/iterator/iterator_traits.h>
#include <thrust/binary_search.h>

#include <thrust/for_each.h>
#include <thrust/detail/backend/dereference.h>
#include <thrust/system/detail/generic/scalar/binary_search.h>

#include <thrust/detail/temporary_array.h>
#include <thrust/detail/type_traits.h>

namespace thrust
{
namespace system
{
namespace detail
{
namespace generic
{
namespace detail
{


// short names to avoid nvcc bug
struct lbf
{
    template <typename RandomAccessIterator, typename T, typename StrictWeakOrdering>
    __host__ __device__
    typename thrust::iterator_traits<RandomAccessIterator>::difference_type
    operator()(RandomAccessIterator begin, RandomAccessIterator end, const T& value, StrictWeakOrdering comp)
    {
        return thrust::system::detail::generic::scalar::lower_bound(begin, end, value, comp) - begin;
    }
};

struct ubf
{
    template <typename RandomAccessIterator, typename T, typename StrictWeakOrdering>
        __host__ __device__
        typename thrust::iterator_traits<RandomAccessIterator>::difference_type
     operator()(RandomAccessIterator begin, RandomAccessIterator end, const T& value, StrictWeakOrdering comp){
         return thrust::system::detail::generic::scalar::upper_bound(begin, end, value, comp) - begin;
     }
};

struct bsf
{
    template <typename RandomAccessIterator, typename T, typename StrictWeakOrdering>
        __host__ __device__
     bool operator()(RandomAccessIterator begin, RandomAccessIterator end, const T& value, StrictWeakOrdering comp){
         RandomAccessIterator iter = thrust::system::detail::generic::scalar::lower_bound(begin, end, value, comp);
         return iter != end && !comp(value, thrust::detail::backend::dereference(iter));
     }
};


template <typename ForwardIterator, typename StrictWeakOrdering, typename BinarySearchFunction>
struct binary_search_functor
{
    ForwardIterator begin;
    ForwardIterator end;
    StrictWeakOrdering comp;
    BinarySearchFunction func;

    binary_search_functor(ForwardIterator begin, ForwardIterator end, StrictWeakOrdering comp, BinarySearchFunction func)
        : begin(begin), end(end), comp(comp), func(func) {}

    template <typename Tuple>
        __host__ __device__
        void operator()(Tuple t)
        {
            thrust::get<1>(t) = func(begin, end, thrust::get<0>(t), comp);
        }
}; // binary_search_functor


// Vector Implementation
template <typename ForwardIterator, typename InputIterator, typename OutputIterator, typename StrictWeakOrdering, typename BinarySearchFunction>
OutputIterator binary_search(ForwardIterator begin, 
                             ForwardIterator end,
                             InputIterator values_begin, 
                             InputIterator values_end,
                             OutputIterator output,
                             StrictWeakOrdering comp,
                             BinarySearchFunction func)
{
    thrust::for_each(thrust::make_zip_iterator(thrust::make_tuple(values_begin, output)),
                     thrust::make_zip_iterator(thrust::make_tuple(values_end, output + thrust::distance(values_begin, values_end))),
                     detail::binary_search_functor<ForwardIterator, StrictWeakOrdering, BinarySearchFunction>(begin, end, comp, func));

    return output + thrust::distance(values_begin, values_end);
}

   

// Scalar Implementation
template <typename OutputType, typename ForwardIterator, typename T, typename StrictWeakOrdering, typename BinarySearchFunction>
OutputType binary_search(ForwardIterator begin,
                         ForwardIterator end,
                         const T& value, 
                         StrictWeakOrdering comp,
                         BinarySearchFunction func)
{
    typedef typename thrust::iterator_space<ForwardIterator>::type Space;

    // use the vectorized path to implement the scalar version

    // allocate device buffers for value and output
    thrust::detail::temporary_array<T,Space>          d_value(1);
    thrust::detail::temporary_array<OutputType,Space> d_output(1);

    // copy value to device
    d_value[0] = value;

    // perform the query
    thrust::system::detail::generic::detail::binary_search(begin, end, d_value.begin(), d_value.end(), d_output.begin(), comp, func);

    // copy result to host and return
    return d_output[0];
}
   
} // end namespace detail


//////////////////////
// Scalar Functions //
//////////////////////

template <typename ForwardIterator, typename T>
ForwardIterator lower_bound(tag,
                            ForwardIterator begin,
                            ForwardIterator end,
                            const T& value)
{
  return thrust::lower_bound(begin, end, value, thrust::less<T>());
}

template <typename ForwardIterator, typename T, typename StrictWeakOrdering>
ForwardIterator lower_bound(tag,
                            ForwardIterator begin,
                            ForwardIterator end,
                            const T& value, 
                            StrictWeakOrdering comp)
{
  typedef typename thrust::iterator_traits<ForwardIterator>::difference_type difference_type;
  
  return begin + detail::binary_search<difference_type>(begin, end, value, comp, detail::lbf());
}


template <typename ForwardIterator, typename T>
ForwardIterator upper_bound(tag,
                            ForwardIterator begin,
                            ForwardIterator end,
                            const T& value)
{
  return thrust::upper_bound(begin, end, value, thrust::less<T>());
}

template <typename ForwardIterator, typename T, typename StrictWeakOrdering>
ForwardIterator upper_bound(tag,
                            ForwardIterator begin,
                            ForwardIterator end,
                            const T& value, 
                            StrictWeakOrdering comp)
{
  typedef typename thrust::iterator_traits<ForwardIterator>::difference_type difference_type;
  
  return begin + detail::binary_search<difference_type>(begin, end, value, comp, detail::ubf());
}


template <typename ForwardIterator, typename T>
bool binary_search(tag,
                   ForwardIterator begin,
                   ForwardIterator end,
                   const T& value)
{
  return thrust::binary_search(begin, end, value, thrust::less<T>());
}

template <typename ForwardIterator, typename T, typename StrictWeakOrdering>
bool binary_search(tag,
                   ForwardIterator begin,
                   ForwardIterator end,
                   const T& value, 
                   StrictWeakOrdering comp)
{
  return detail::binary_search<bool>(begin, end, value, comp, detail::bsf());
}


//////////////////////
// Vector Functions //
//////////////////////

template <typename ForwardIterator, typename InputIterator, typename OutputIterator>
OutputIterator lower_bound(tag,
                           ForwardIterator begin, 
                           ForwardIterator end,
                           InputIterator values_begin, 
                           InputIterator values_end,
                           OutputIterator output)
{
  typedef typename thrust::iterator_value<InputIterator>::type ValueType;

  return thrust::lower_bound(begin, end, values_begin, values_end, output, thrust::less<ValueType>());
}

template <typename ForwardIterator, typename InputIterator, typename OutputIterator, typename StrictWeakOrdering>
OutputIterator lower_bound(tag,
                           ForwardIterator begin, 
                           ForwardIterator end,
                           InputIterator values_begin, 
                           InputIterator values_end,
                           OutputIterator output,
                           StrictWeakOrdering comp)
{
  return detail::binary_search(begin, end, values_begin, values_end, output, comp, detail::lbf());
}


template <typename ForwardIterator, typename InputIterator, typename OutputIterator>
OutputIterator upper_bound(tag,
                           ForwardIterator begin, 
                           ForwardIterator end,
                           InputIterator values_begin, 
                           InputIterator values_end,
                           OutputIterator output)
{
  typedef typename thrust::iterator_value<InputIterator>::type ValueType;

  return thrust::upper_bound(begin, end, values_begin, values_end, output, thrust::less<ValueType>());
}

template <typename ForwardIterator, typename InputIterator, typename OutputIterator, typename StrictWeakOrdering>
OutputIterator upper_bound(tag,
                           ForwardIterator begin, 
                           ForwardIterator end,
                           InputIterator values_begin, 
                           InputIterator values_end,
                           OutputIterator output,
                           StrictWeakOrdering comp)
{
  return detail::binary_search(begin, end, values_begin, values_end, output, comp, detail::ubf());
}


template <typename ForwardIterator, typename InputIterator, typename OutputIterator>
OutputIterator binary_search(tag,
                             ForwardIterator begin, 
                             ForwardIterator end,
                             InputIterator values_begin, 
                             InputIterator values_end,
                             OutputIterator output)
{
  typedef typename thrust::iterator_value<InputIterator>::type ValueType;

  return thrust::binary_search(begin, end, values_begin, values_end, output, thrust::less<ValueType>());
}

template <typename ForwardIterator, typename InputIterator, typename OutputIterator, typename StrictWeakOrdering>
OutputIterator binary_search(tag,
                             ForwardIterator begin, 
                             ForwardIterator end,
                             InputIterator values_begin, 
                             InputIterator values_end,
                             OutputIterator output,
                             StrictWeakOrdering comp)
{
  return detail::binary_search(begin, end, values_begin, values_end, output, comp, detail::bsf());
}


template <typename ForwardIterator, typename LessThanComparable>
thrust::pair<ForwardIterator,ForwardIterator>
equal_range(tag,
            ForwardIterator first,
            ForwardIterator last,
            const LessThanComparable &value)
{
  return thrust::equal_range(first, last, value, thrust::less<LessThanComparable>());
}


template <typename ForwardIterator, typename T, typename StrictWeakOrdering>
thrust::pair<ForwardIterator,ForwardIterator>
equal_range(tag,
            ForwardIterator first,
            ForwardIterator last,
            const T &value,
            StrictWeakOrdering comp)
{
  ForwardIterator lb = thrust::lower_bound(first, last, value, comp);
  ForwardIterator ub = thrust::upper_bound(first, last, value, comp);
  return thrust::make_pair(lb, ub);
}


} // end namespace generic
} // end namespace detail
} // end namespace system
} // end namespace thrust
