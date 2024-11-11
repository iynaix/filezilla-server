#ifndef FZ_UTIL_VECTOR_MAP_HPP
#define FZ_UTIL_VECTOR_MAP_HPP

#include <vector>
#include <algorithm>

namespace fz::util
{
	namespace vector_map_detail {
		template <auto Key>
		struct get_record;

		template <typename Record, typename KeyType, KeyType Record::*KeyField>
		struct get_record<KeyField> {
			using type = Record;
		};
	}

	template <auto KeyField, typename Record = typename vector_map_detail::get_record<KeyField>::type, std::enable_if_t<std::is_class_v<Record>>* = nullptr>
	class vector_map {
	public:
		using key_type = std::decay_t<decltype(std::declval<Record>().*KeyField)>;

		using iterator = typename std::vector<Record>::const_iterator;
		using const_iterator = typename std::vector<Record>::const_iterator;

		vector_map() = default;
		vector_map(std::vector<Record> && vec) {
			*this = std::move(vec);
		}

		vector_map &operator=(std::vector<Record> && vec) {
			vec_ = std::move(vec);

			std::sort(vec_.begin(), vec_.end(), Comparator());

			return *this;
		}

		const_iterator find(const_iterator begin, const key_type &key) const {
			auto end = cend();

			// Finds the lower bound in at most log(last - first) + 1 comparisons
			auto i = std::lower_bound(begin, end, key, Comparator());

			if (i != end && !(Comparator()(key, *i)))
				return i; // found
			else
				return end; // not found
		}

		const_iterator find(const key_type &key) const {
			return find(cbegin(), key);
		}


		const_iterator cbegin() const { return vec_.cbegin(); }
		const_iterator cend() const { return vec_.cend(); }

		const_iterator begin() const { return vec_.cbegin(); }
		const_iterator end() const { return vec_.cend(); }

		const std::vector<Record> &data() const { return vec_; }

	private:
		struct Comparator {
			bool operator()(const Record &lhs, const Record &rhs) {
				return lhs.*KeyField < rhs.*KeyField;
			}

			bool operator()(const key_type &lhs_key, const Record &rhs) {
				return lhs_key < rhs.*KeyField;
			}

			bool operator()(const Record &lhs, const key_type &rhs_key) {
				return lhs.*KeyField < rhs_key;
			}
		};

		std::vector<Record> vec_;
	};

}

#endif // VECTOR_MAP_HPP
