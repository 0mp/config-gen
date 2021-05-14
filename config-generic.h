#pragma once

#include <algorithm>
#include <assert.h>
#include <chrono>
#include <initializer_list>
#include <string_view>
#include <ucl.h>
#include <unordered_map>
#include <utility>

#include <stdio.h>

namespace config::detail
{
	class UCLPtr
	{
		const ucl_object_t *obj = nullptr;

		public:
		UCLPtr() {}
		UCLPtr(const ucl_object_t *o) : obj(ucl_object_ref(o)) {}
		UCLPtr(const UCLPtr &o) : obj(ucl_object_ref(o.obj)) {}
		UCLPtr(UCLPtr &&o) : obj(o.obj)
		{
			o.obj = nullptr;
		}
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
		UCLPtr operator[](const char *key) const
		{
			return ucl_object_lookup(obj, key);
		}
	};

	template<typename T, typename Adaptor = T, bool IterateProperties = false>
	class Range
	{
		UCLPtr                array;
		enum ucl_iterate_type iterate_type;
		class Iter
		{
			ucl_object_iter_t     iter{nullptr};
			UCLPtr                obj;
			UCLPtr                array;
			enum ucl_iterate_type iterate_type;

			public:
			Iter() : iter(nullptr), obj(nullptr), array(nullptr) {}
			Iter(const Iter &) = delete;
			Iter(Iter &&)      = delete;
			Iter(const ucl_object_t *arr, const ucl_iterate_type type)
			  : array(arr), iterate_type(type)
			{
				if (!IterateProperties && (ucl_object_type(array) != UCL_ARRAY))
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
		~Range() {}
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
			return (array == nullptr) || (ucl_object_type(array) == UCL_NULL);
		}
	};

	class StringViewAdaptor
	{
		const ucl_object_t *obj;

		public:
		StringViewAdaptor(const ucl_object_t *o) : obj(o) {}
		operator std::string_view()
		{
			const char *cstr = ucl_object_tostring(obj);
			if (cstr != nullptr)
			{
				return cstr;
			}
			return std::string_view("", 0);
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
	using DoubleAdaptor = NumberAdaptor<double, ucl_object_todouble>;

	template<size_t N>
	struct StringLiteral
	{
		constexpr StringLiteral(const char (&str)[N])
		{
			std::copy_n(str, N, value);
		}
		operator std::string_view() const
		{
			// Strip out the null terminator
			return {value, N - 1};
		}
		operator const char *() const
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

	template<StringLiteral Key, typename Adaptor>
	struct NamedType
	{
		static Adaptor construct(const ucl_object_t *obj)
		{
			return Adaptor{obj};
		}

		static constexpr std::string_view key()
		{
			return Key;
		}
	};

	template<StringLiteral KeyName, typename... Types>
	class NamedTypeAdaptor
	{
		UCLPtr obj;
		using TypesAsTuple = std::tuple<Types...>;
		template<typename T, size_t I = 0>
		constexpr void visit_impl(std::string_view key, T &&v)
		{
			if (key == std::tuple_element_t<I, TypesAsTuple>::key())
			{
				return v(std::tuple_element_t<I, TypesAsTuple>::construct(obj));
			}
			if constexpr (I + 1 < std::tuple_size_v<TypesAsTuple>)
			{
				return visit_impl<T, I + 1>(key, std::forward<T &&>(v));
			}
		}

		template<class... Fs>
		struct Dispatcher : Fs...
		{
			template<class... Ts>
			Dispatcher(Ts &&...ts) : Fs{std::forward<Ts>(ts)}...
			{
			}

			using Fs::operator()...;
		};
		template<class... Ts>
		Dispatcher(Ts &&...) -> Dispatcher<std::remove_reference_t<Ts>...>;

		public:
		NamedTypeAdaptor(const ucl_object_t *o) : obj(o) {}
		template<typename... Visitors>
		void visit(Visitors &&...visitors)
		{
			visit_impl(StringViewAdaptor(obj[KeyName]),
			           Dispatcher{visitors...});
		}
		template<typename... Visitors>
		void visit_some(Visitors &&...visitors)
		{
			visit_impl(StringViewAdaptor(obj[KeyName]),
			           Dispatcher{visitors..., [](auto &&) {}});
		}
	};

	template<typename Adaptor>
	class PropertyAdaptor : public Adaptor
	{
		public:
		PropertyAdaptor(const ucl_object_t *o) : Adaptor(o) {}
		std::string_view key()
		{
			return ucl_object_key(Adaptor::obj);
		}
	};

	template<typename Adaptor, typename T = Adaptor>
	std::optional<T> make_optional(const ucl_object_t *o)
	{
		if (o == nullptr)
		{
			return {};
		}
		return Adaptor(o);
	}

} // namespace config::detail
