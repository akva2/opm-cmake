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
#ifndef SERIALIZER_HPP
#define SERIALIZER_HPP

#include <opm/common/utility/CheckSum.hpp>

#include <boost/crc.hpp>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#if HAVE_DUNE_COMMON
namespace Dune { template<typename,int> class FieldVector; }
#endif

#if HAVE_DUNE_ISTL
namespace Dune { template<typename,typename> class BlockVector; }
#endif

namespace Opm {
namespace detail {

template<typename ...Ts>
struct MakeVariantImpl
{

template<std::size_t Index, typename, typename ...Rest>
static decltype(auto) make_variant(std::size_t index)
{
  if(Index == index)
    return std::variant<Ts...>{std::in_place_index_t<Index>{}};

  if constexpr(sizeof...(Rest) != 0)
    return make_variant<Index + 1, Rest...>(index);
  else
    throw std::runtime_error("Invalid variant index");
}

};

template<typename ...Ts>
decltype(auto) make_variant(std::size_t index)
{
  return detail::MakeVariantImpl<Ts...>::template make_variant<0, Ts...>(index);
}

template<class T>
using remove_cvr_t = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename T>
struct is_unique_ptr : std::false_type {};

template <typename T>
struct is_unique_ptr<std::unique_ptr<T>> : std::true_type {};

} // namespace detail

/*! \brief Class for (de-)serializing.
 *!  \details If the class has a serializeOp member this is used,
 *            if not it is passed on to the underlying packer.
*/

template<class Packer>
class Serializer {
public:
    //! \brief Constructor.
    //! \param packer Packer to use
    //! \param checksum True to use check-summing
    Serializer(const Packer& packer, bool checksum)
        : m_packer(packer)
        , m_checksum(m_checksummer)
        , m_use_checksum(checksum)
    {}

    //! \brief Applies current serialization op to the passed data.
    template<class T>
    void operator()(const T& data)
    {
        if constexpr (is_ptr<T>::value) {
            if constexpr (detail::is_unique_ptr<T>::value) {
                uniqueptr(data);
            } else {
                ptr(data);
            }
        } else if constexpr (is_pair_or_tuple<T>::value) {
            tuple(data);
        } else if constexpr (is_variant<T>::value) {
            variant(data);
        } else if constexpr (is_optional<T>::value) {
            optional(data);
        } else if constexpr (is_vector<T>::value) {
            vector(data);
        } else if constexpr (is_map<T>::value) {
            map(data);
        } else if constexpr (is_array<T>::value) {
            array(data);
        } else if constexpr (is_set<T>::value) {
            set(data);
        } else if constexpr (has_serializeOp<detail::remove_cvr_t<T>>::value) {
            const_cast<T&>(data).serializeOp(*this);
        } else {
            switch (m_op) {
            case Operation::PACKSIZE:
                m_packSize += m_packer.packSize(data);
                break;
            case Operation::PACK:
                m_packer.pack(data, m_buffer, m_position);
                if (m_use_checksum && !m_suspend_check_sum) {
                    m_checksum.checkSum(data);
                }
                break;
            case Operation::UNPACK:
                m_packer.unpack(const_cast<T&>(data), m_buffer, m_position);
                if (m_use_checksum && !m_suspend_check_sum) {
                    m_checksum.checkSum(data);
                }
                break;
            case Operation::CHECKSUM:
                m_checksum.checkSum(data);
                break;
            }
        }
    }

    //! \brief Call this to serialize data.
    //! \tparam T Type of class to serialize
    //! \param data Class to serialize
    template<class T>
    void pack(const T& data)
    {
        m_ptrmap.clear();
        m_op = Operation::PACKSIZE;
        m_packSize = 0;
        (*this)(data);
        m_position = 0;
        if (m_use_checksum) {
            m_packSize += sizeof(uint32_t);
            m_checksummer.reset();
        }
        m_buffer.resize(m_packSize);
        m_ptrmap.clear();
        m_op = Operation::PACK;
        (*this)(data);
        if (m_use_checksum) {
            m_suspend_check_sum = true;
            (*this)(m_checksummer.checksum());
            m_suspend_check_sum = false;
        }
    }

    //! \brief Call this to serialize data.
    //! \tparam T Type of class to serialize
    //! \param data Class to serialize
    template<class... Args>
    void pack(const Args&... data)
    {
        m_ptrmap.clear();
        m_op = Operation::PACKSIZE;
        m_packSize = 0;
        variadic_call(data...);
        if (m_use_checksum) {
            m_packSize += sizeof(uint32_t);
            m_checksummer.reset();
        }
        m_position = 0;
        m_buffer.resize(m_packSize);
        m_ptrmap.clear();
        m_op = Operation::PACK;
        variadic_call(data...);
        if (m_use_checksum) {
            m_suspend_check_sum = true;
            (*this)(m_checksummer.checksum());
            m_suspend_check_sum = false;
        }
    }

    //! \brief Call this to de-serialize data.
    //! \tparam T Type of class to de-serialize
    //! \param data Class to de-serialize
    template<class T>
    void unpack(T& data)
    {
        m_position = 0;
        m_ptrmap.clear();
        m_op = Operation::UNPACK;
        if (m_use_checksum) {
            m_checksummer.reset();
        }
        (*this)(data);
        if (m_use_checksum) {
            m_suspend_check_sum = true;
            uint32_t check_sum;
            (*this)(check_sum);
            if (check_sum != m_checksummer.checksum()) {
                throw std::runtime_error("Check-sum mismatch in de-serialization");
            }
            m_suspend_check_sum = false;
        }
    }

    //! \brief Call this to de-serialize data.
    //! \tparam T Type of class to de-serialize
    //! \param data Class to de-serialize
    template<class... Args>
    void unpack(Args&... data)
    {
        m_position = 0;
        m_ptrmap.clear();
        m_op = Operation::UNPACK;
        if (m_use_checksum) {
            m_checksummer.reset();
        }
        variadic_call(data...);
        if (m_use_checksum) {
            m_suspend_check_sum = true;
            uint32_t check_sum;
            (*this)(check_sum);
            if (check_sum != m_checksummer.checksum()) {
                throw std::runtime_error("Check-sum mismatch in de-serialization");
            }
            m_suspend_check_sum = false;
        }
    }

    //! \brief Call this to calculate check-sum of data.
    //! \tparam T Type of class to check-sum
    //! param data Class to check-sum
    template<class T>
    uint32_t checksum(const T& data)
    {
        m_op = Operation::CHECKSUM;
        m_checksummer.reset();
        m_ptrmap.clear();
        (*this)(data);
        return m_checksummer.checksum();
    }

    template<class T>
    void appendCheckSum(const T& data)
    {
        Operation op = Operation::CHECKSUM;
        std::swap(op, m_op);
        (*this)(data);
        std::swap(op, m_op);
    }

    //! \brief Call this to calculate check-sum of data.
    //! \tparam T Type of class to check-sum
    //! param data Class to check-sum
    template<class...  Args>
    uint32_t checksum(const Args&... data)
    {
        m_op = Operation::CHECKSUM;
        m_checksummer.reset();
        m_ptrmap.clear();
        variadic_call(data...);
        return m_checksummer.checksum();
    }

    //! \brief Returns current position in buffer.
    size_t position() const
    {
        return m_position;
    }

    //! \brief Returns true if we are currently doing a serialization operation.
    bool isSerializing() const
    {
        return m_op != Operation::UNPACK;
    }

    //! \brief Returns true if we are currently doing a check-summing operation.
    bool isCheckSumming() const
    {
        return m_op == Operation::CHECKSUM;
    }

    //! \brief Suspend automatic check-summing if enabled.
    //! \return True if check-summing is enabled
    bool manualCheckSumming(bool manual)
    {
        if (m_use_checksum) {
            m_suspend_check_sum = manual;
        }

        return m_use_checksum;
    }

protected:
    //! \brief Handler for vectors.
    //! \tparam T Type for vector elements
    //! \param data The vector to (de-)serialize
    template <typename Vector>
    void vector(const Vector& data)
    {
        if constexpr (std::is_pod_v<typename Vector::value_type>) {
          if (m_op == Operation::PACKSIZE) {
              (*this)(data.size());
              if (data.size() > 0) {
                  m_packSize += m_packer.packSize(data.data(), data.size());
              }
          } else if (m_op == Operation::PACK) {
              (*this)(data.size());
              if (data.size() > 0) {
                  m_packer.pack(data.data(), data.size(), m_buffer, m_position);
              }
          } else if (m_op == Operation::UNPACK) {
              std::size_t size = 0;
              (*this)(size);
              auto& data_mut = const_cast<Vector&>(data);
              data_mut.resize(size);
              if (size > 0) {
                  m_packer.unpack(data_mut.data(), size, m_buffer, m_position);
              }
          }
        } else {
            if (m_op == Operation::UNPACK) {
                std::size_t size = 0;
                (*this)(size);
                auto& data_mut = const_cast<Vector&>(data);
                data_mut.resize(size);
                std::for_each(data_mut.begin(), data_mut.end(), std::ref(*this));
            } else {
                (*this)(data.size());
                std::for_each(data.begin(), data.end(), std::ref(*this));
            }
        }
    }

    //! \brief Handler for bool vectors.
    //! \param data The vector to (de-)serialize
    void vector(const std::vector<bool>& data)
    {
        if (m_op == Operation::UNPACK) {
            std::size_t size = 0;
            (*this)(size);
            auto& data_mut = const_cast<std::vector<bool>&>(data);
            data_mut.clear();
            data_mut.reserve(size);
            for (size_t i = 0; i < size; ++i) {
                bool entry = false;
                (*this)(entry);
                data_mut.push_back(entry);
            }
        } else {
            (*this)(data.size());
            for (const auto entry : data) { // Not a reference: vector<bool> range
                bool b = entry;
                (*this)(b);
            }
        }
    }

    //! \brief Handler for arrays.
    //! \param data The array to (de-)serialize
    template <class Array>
    void array(const Array& data)
    {
        using T = typename Array::value_type;

        if constexpr (std::is_pod_v<T>) {
            if (m_op == Operation::PACKSIZE)
                m_packSize += m_packer.packSize(data.data(), data.size());
            else if (m_op == Operation::PACK)
                m_packer.pack(data.data(), data.size(), m_buffer, m_position);
            else if (m_op == Operation::UNPACK) {
                auto& data_mut = const_cast<Array&>(data);
                m_packer.unpack(data_mut.data(), data_mut.size(), m_buffer, m_position);
            }
        } else {
            std::for_each(data.begin(), data.end(), std::ref(*this));
        }
    }

    //! \brief Handler for std::variant.
    //! \param data The variant to (de-)serialize
    template<class... Args>
    void variant(const std::variant<Args...>& data)
    {
        if (m_op == Operation::UNPACK) {
            std::size_t index = 0;
            (*this)(index);
            auto& data_mut = const_cast<std::variant<Args...>&>(data);
            data_mut = detail::make_variant<Args...>(index);
            std::visit(std::ref(*this), data_mut);
        } else {
            (*this)(data.index());
            std::visit(std::ref(*this), data);
        }
    }

    //! \brief Handler for std::optional.
    //! \tparam T Type for data
    //! \param data The optional to (de-)serialize
    template<class T>
    void optional(const std::optional<T>& data)
    {
        if (m_op == Operation::UNPACK) {
            bool has = false;
            (*this)(has);
            if (has) {
                T res{};
                (*this)(res);
                const_cast<std::optional<T>&>(data) = res;
            } else {
                const_cast<std::optional<T>&>(data) = std::nullopt;
            }
        } else {
            (*this)(data.has_value());
            if (data.has_value()) {
                (*this)(*data);
            }
        }
    }

    //! \brief Handler for std::tuple.
    //! \param data The tuple to (de-)serialize
    template<class Tuple>
    void tuple(const Tuple& data)
    {
        tuple_call(data);
    }

    //! \brief Handler for maps.
    //! \tparam Map map type
    //! \param map The map to (de-)serialize
    template<class Map>
    void map(const Map& data)
    {
        if (m_op == Operation::UNPACK) {
            std::size_t size = 0;
            (*this)(size);
            auto& data_mut = const_cast<Map&>(data);
            for (size_t i = 0; i < size; ++i) {
                typename Map::value_type entry;
                (*this)(entry);
                data_mut.insert(entry);
            }
        } else if (m_op == Operation::CHECKSUM) {
            (*this)(data.size());
            for (std::size_t i : getSortedIndex(data)) {
                auto it = data.begin();
                std::advance(it, i);
                (*this)(*it);
            }
        } else {
            (*this)(data.size());
            std::for_each(data.begin(), data.end(), std::ref(*this));
        }
    }

    //! \brief Handler for sets.
    //! \tparam Set set type
    //! \param data The set to (de-)serialize
    template<class Set>
    void set(const Set& data)
    {
        if (m_op == Operation::UNPACK) {
            std::size_t size = 0;
            (*this)(size);
            auto& data_mut = const_cast<Set&>(data);
            for (size_t i = 0; i < size; ++i) {
                typename Set::value_type entry{};
                (*this)(entry);
                data_mut.insert(entry);
            }
        } else if (m_op == Operation::CHECKSUM) {
            std::set<typename Set::value_type> sset;
            for (const auto& d : data) {
                sset.insert(d);
            }
            (*this)(data.size());
            for (auto& i : sset)
                (*this)(i);
        } else {
            (*this)(data.size());
            std::for_each(data.begin(), data.end(), std::ref(*this));
        }
    }

    template<typename T, typename... Args>
    void variadic_call(T& first,
                       Args&&... args)
    {
      (*this)(first);
      if constexpr (sizeof...(args) > 0)
          variadic_call(std::forward<Args>(args)...);
    }

    template<std::size_t I = 0, typename Tuple>
    typename std::enable_if<I == std::tuple_size<Tuple>::value, void>::type
    tuple_call(const Tuple&)
    {
    }

    template<std::size_t I = 0, typename Tuple>
    typename std::enable_if<I != std::tuple_size<Tuple>::value, void>::type
    tuple_call(const Tuple& tuple)
    {
        (*this)(std::get<I>(tuple));
        tuple_call<I+1>(tuple);
    }

    //! \brief Enumeration of operations.
    enum class Operation {
        PACKSIZE, //!< Calculating serialization buffer size
        PACK,     //!< Performing serialization
        UNPACK,   //!< Performing de-serialization
        CHECKSUM  //!< Performing check-summing
    };

    //! \brief Predicate for detecting vectors.
    template<class T>
    struct is_vector {
        constexpr static bool value = false;
    };

    template<class T1, class Allocator>
    struct is_vector<std::vector<T1,Allocator>> {
        constexpr static bool value = true;
    };

#if HAVE_DUNE_ISTL
    template<class T1, class Allocator>
    struct is_vector<Dune::BlockVector<T1,Allocator>> {
        constexpr static bool value = true;
    };
#endif

    //! \brief Predicate for detecting variants.
    template<class T>
    struct is_variant {
        constexpr static bool value = false;
    };

    template<class... Ts>
    struct is_variant<std::variant<Ts...>> {
        constexpr static bool value = true;
    };

    //! \brief Predicate for detecting pairs and tuples.
    template<class T>
    struct is_pair_or_tuple {
        constexpr static bool value = false;
    };

    template<class... Ts>
    struct is_pair_or_tuple<std::tuple<Ts...>> {
        constexpr static bool value = true;
    };

    template<class T1, class T2>
    struct is_pair_or_tuple<std::pair<T1,T2>> {
        constexpr static bool value = true;
    };

    //! \brief Predicate for smart pointers.
    template<class T>
    struct is_ptr {
        constexpr static bool value = false;
    };

    template<class T1>
    struct is_ptr<std::shared_ptr<T1>> {
        constexpr static bool value = true;
    };

    template<class T1, class Deleter>
    struct is_ptr<std::unique_ptr<T1, Deleter>> {
        constexpr static bool value = true;
    };

    //! \brief Predicate for std::optional.
    template<class T>
    struct is_optional {
        constexpr static bool value = false;
    };

    template<class T1>
    struct is_optional<std::optional<T1>> {
        constexpr static bool value = true;
    };

    //! \brief Predicate for maps
    template<class T>
    struct is_map {
        constexpr static bool value = false;
    };

    template<class Key, class T, class Compare, class Allocator>
    struct is_map<std::map<Key,T,Compare,Allocator>> {
        constexpr static bool value = true;
    };

    template<class Key, class T, class Hash, class KeyEqual, class Allocator>
    struct is_map<std::unordered_map<Key,T,Hash,KeyEqual,Allocator>> {
        constexpr static bool value = true;
    };

    //! \brief Predicate for sets
    template<class T>
    struct is_set {
        constexpr static bool value = false;
    };

    template<class Key, class Compare, class Allocator>
    struct is_set<std::set<Key,Compare,Allocator>> {
        constexpr static bool value = true;
    };

    template<class Key, class Hash, class KeyEqual, class Allocator>
    struct is_set<std::unordered_set<Key,Hash,KeyEqual,Allocator>> {
        constexpr static bool value = true;
    };

    //! \brief Predicate for arrays
    template<class T>
    struct is_array {
        constexpr static bool value = false;
    };

    template<class T, std::size_t N>
    struct is_array<std::array<T,N>> {
        constexpr static bool value = true;
    };

#if HAVE_DUNE_COMMON
    template<class T, int N>
    struct is_array<Dune::FieldVector<T,N>> {
        constexpr static bool value = true;
    };
#endif

    //! Detect existence of \c serializeOp member function
    //!
    //! Base case (no \c serializeOp member function)
    template <typename, class = void>
    struct has_serializeOp : public std::false_type {};

    //! Detect existence of \c serializeOp member function
    //!
    //! Non-default, albeit common, case (type has \c serializeOp member
    //! function)
    template <typename T>
    struct has_serializeOp<
        T, std::void_t<decltype(std::declval<T>().serializeOp(std::declval<Serializer<Packer>&>()))>
    > : public std::true_type {};

    //! \brief Handler for smart pointers.
    template<class PtrType>
    void ptr(const PtrType& data)
    {
        using T1 = typename PtrType::element_type;
        void* data_ptr = reinterpret_cast<void*>(data.get());
        if (m_op != Operation::CHECKSUM) {
            (*this)(data_ptr);
            if (!data_ptr)
                return;
          }

        if (m_op == Operation::PACK || m_op == Operation::PACKSIZE) {
            if (m_ptrmap.count(data_ptr) == 0) {
                (*this)(*data);
                m_ptrmap[data_ptr] = nullptr;
            }
        } else if (m_op == Operation::CHECKSUM) {
            (*this)(data ? 1 : 0);
            if (data) {
                (*this)(*data);
            }
        } else {  // m_op == Operation::UNPACK
            if (m_ptrmap.count(data_ptr) == 0) {
                const_cast<PtrType&>(data).reset(new T1);
                m_ptrmap[data_ptr] = reinterpret_cast<void*>(& const_cast<PtrType&>(data));
                (*this)(*data);
            } else {
                const_cast<PtrType&>(data) = *(reinterpret_cast<PtrType*>(m_ptrmap[data_ptr]));
            }
        }
    }

    template<class PtrType>
    void uniqueptr(const PtrType& data)
    {
        using T1 = typename PtrType::element_type;
        void* data_ptr = reinterpret_cast<void*>(data.get());
        if (m_op != Operation::CHECKSUM) {
            (*this)(data_ptr);
            if (!data_ptr)
                return;
        }

        if (m_op == Operation::PACK || m_op == Operation::PACKSIZE) {
            if (m_ptrmap.count(data_ptr) == 0) {
                (*this)(*data);
                m_ptrmap[data_ptr] = nullptr;
            }
        } else if (m_op == Operation::CHECKSUM) {
            (*this)(data ? 1 : 0);
            if (data) {
                (*this)(*data);
            }
        } else {  // m_op == Operation::UNPACK
            if (m_ptrmap.count(data_ptr) == 0) {
                const_cast<PtrType&>(data).reset(new T1);
                m_ptrmap[data_ptr] = reinterpret_cast<void*>(& const_cast<PtrType&>(data));
                (*this)(*data);
            } else {
                throw std::runtime_error("Should never have to reconstruct more than one unique_ptr!");
            }
        }
    }

    template<class Container>
    std::vector<std::size_t> getSortedIndex(const Container& C)
    {
        std::vector<std::size_t> index(C.size());
        std::iota(index.begin(), index.end(), 0);
        std::sort(index.begin(), index.end(),
                  [&C](std::size_t i, std::size_t j)
                  {
                      auto it1 = C.begin();
                      std::advance(it1, i);
                      auto it2 = C.begin();
                      std::advance(it2, j);
                      return it1->first < it2->first;
                  });

        return index;
    }

    const Packer& m_packer; //!< Packer to use
    Operation m_op = Operation::PACKSIZE; //!< Current operation
    size_t m_packSize = 0; //!< Required buffer size after PACKSIZE has been done
    size_t m_position = 0; //!< Current position in buffer
    std::vector<char> m_buffer; //!< Buffer for serialized data
    std::map<void*, void*> m_ptrmap; //!< Map to keep track of which pointer data has been serialized and actual pointers during unpacking
    Serialization::CheckSum<boost::crc_32_type> m_checksum;
    boost::crc_32_type m_checksummer; //!< Checksum calculator
    bool m_use_checksum; //!< True to calculate check-sum during pack/unpack
    bool m_suspend_check_sum = false; //!< True to suspend automatic check-summing
};

}

#endif
