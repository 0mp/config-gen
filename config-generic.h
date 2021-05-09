#pragma once

#include <algorithm>
#include <assert.h>
#include <chrono>
#include <initializer_list>
#include <string_view>
#include <ucl.h>
#include <unordered_map>
#include <utility>

namespace config
{
	namespace detail
	{
		class UCLPtr
		{
			const ucl_object_t *obj;

			public:
			UCLPtr(const ucl_object_t *o) : obj(ucl_object_ref(o)) {}
			~UCLPtr()
			{
				ucl_object_unref(const_cast<ucl_object_t *>(obj));
			}
			UCLPtr &operator=(const ucl_object_t *o)
			{
				o = ucl_object_ref(o);
				ucl_object_unref(const_cast<ucl_object_t *>(obj));
				obj = o;
				return *this;
			}
			operator const ucl_object_t *() const
			{
				return obj;
			}
			bool operator!=(const ucl_object_t *o)
			{
				return obj != o;
			}
			UCLPtr operator[](const char *key)
			{
				return ucl_object_lookup(obj, key);
			}
		};

		template<typename T, typename Adaptor = T>
		class Range
		{
			UCLPtr                array;
			enum ucl_iterate_type iterate_type;
			class Iter
			{
				ucl_object_iter_t     iter{nullptr};
				UCLPtr                obj{nullptr};
				UCLPtr                array;
				enum ucl_iterate_type iterate_type;
				friend class UCLArray;

				public:
				Iter() : iter(nullptr), obj(nullptr), array(nullptr) {}
				Iter(const ucl_object_t *arr, const ucl_iterate_type type)
				  : array(arr), iterate_type(type)
				{
					if (ucl_object_type(array) != UCL_ARRAY)
					{
						array = nullptr;
						obj   = arr;
						return;
					}
					iter = ucl_object_iterate_new(array);
					++(*this);
				}
				T operator->()
				{
					return Adaptor(obj);
				}
				T operator*()
				{
					return Adaptor(obj);
				}
				bool operator!=(const Iter &other)
				{
					return obj != other.obj;
				}
				Iter &operator++()
				{
					if (iter == nullptr)
					{
						obj = nullptr;
					}
					else
					{
						obj = ucl_object_iterate_safe(iter, iterate_type);
					}
					return *this;
				}
				~Iter()
				{
					if (iter != nullptr)
					{
						ucl_object_iterate_free(iter);
					}
				}
			};

			public:
			Range(const ucl_object_t *   arr,
			      const ucl_iterate_type type = UCL_ITERATE_BOTH)
			  : array(arr), iterate_type(type)
			{
			}
			Iter begin()
			{
				return {array, iterate_type};
			}
			Iter end()
			{
				return {};
			}
			bool empty()
			{
				return (array == nullptr) ||
				       (ucl_object_type(array) == UCL_NULL);
			}
		};

		class StringViewAdaptor
		{
			const ucl_object_t *obj;

			public:
			StringViewAdaptor(const ucl_object_t *o) : obj(o) {}
			operator std::string_view()
			{
				return ucl_object_tostring(obj);
			}
		};

		class DurationAdaptor
		{
			const ucl_object_t *obj;

			public:
			DurationAdaptor(const ucl_object_t *o) : obj(o) {}
			operator std::chrono::seconds()
			{
				assert(ucl_object_type(obj) == UCL_TIME);
				return std::chrono::seconds(
				  static_cast<long long>(ucl_object_todouble(obj)));
			}
		};

		template<typename NumberType,
		         auto (*Conversion)(const ucl_object_t *) = ucl_object_toint>
		class NumberAdaptor
		{
			const ucl_object_t *obj;

			public:
			NumberAdaptor(const ucl_object_t *o) : obj(o) {}
			operator NumberType()
			{
				return static_cast<NumberType>(Conversion(obj));
			}
		};

		using UInt64Adaptor = NumberAdaptor<uint64_t>;
		using UInt32Adaptor = NumberAdaptor<uint32_t>;
		using UInt16Adaptor = NumberAdaptor<uint16_t>;
		using UInt8Adaptor  = NumberAdaptor<uint8_t>;
		using Int64Adaptor  = NumberAdaptor<int64_t>;
		using Int32Adaptor  = NumberAdaptor<int32_t>;
		using Int16Adaptor  = NumberAdaptor<int16_t>;
		using Int8Adaptor   = NumberAdaptor<int8_t>;
		using BoolAdaptor   = NumberAdaptor<bool, ucl_object_toboolean>;
		using FloatAdaptor  = NumberAdaptor<float, ucl_object_todouble>;
		using DoubleAdaptor = NumberAdaptor<float, ucl_object_todouble>;

		template<size_t N>
		struct StringLiteral
		{
			constexpr StringLiteral(const char (&str)[N])
			{
				std::copy_n(str, N, value);
			}
			operator std::string_view()
			{
				return value;
			}
			char value[N];
		};

		template<size_t Size, typename EnumType>
		struct Enum
		{
			constexpr Enum(const char (&str)[Size], EnumType e) : val(e)
			{
				std::copy_n(str, Size - 1, key_buffer);
			}
			char     key_buffer[Size - 1] = {0};
			EnumType val;

			constexpr std::string_view key() const
			{
				// Strip out the null terminator
				return {key_buffer, Size - 1};
			}
		};

		template<Enum... kvp>
		class EnumValueMap
		{
			using KVPs = std::tuple<decltype(kvp)...>;
			static constexpr KVPs kvps{kvp...};
			using Value = std::remove_reference_t<decltype(get<0>(kvps).val)>;

			template<size_t Element>
			static constexpr Value lookup(std::string_view key) noexcept
			{
				static_assert(
				  std::is_same_v<
				    Value,
				    std::remove_reference_t<decltype(get<Element>(kvps).val)>>,
				  "All entries must use the same enum value");
				if (key == get<Element>(kvps).key())
				{
					return get<Element>(kvps).val;
				}
				if constexpr (Element + 1 < std::tuple_size_v<KVPs>)
				{
					return lookup<Element + 1>(key);
				}
				else
				{
					return static_cast<Value>(-1);
				}
			}

			public:
			static constexpr Value get(std::string_view key) noexcept
			{
				return lookup<0>(key);
			}
		};

		template<typename EnumType, typename Map>
		class EnumAdaptor
		{
			const ucl_object_t *obj;

			public:
			EnumAdaptor(const ucl_object_t *o) : obj(o) {}
			operator EnumType()
			{
				return Map::get(ucl_object_tostring(obj));
			}
		};
	} // namespace detail
} // namespace config
