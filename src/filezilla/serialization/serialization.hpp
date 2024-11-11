#ifndef FZ_SERIALIZATION_SERIALIZATION_HPP
#define FZ_SERIALIZATION_SERIALIZATION_HPP

#include "../serialization/detail/base.hpp"

namespace fz::serialization
{
	class archive_is_input{};
	class archive_is_output{};
	class archive_is_textual{};

	template <typename Archive, typename T, typename... PostProcess>
	Archive &access::process(Archive &ar, T && t, const PostProcess &... post_process) {
		using namespace trait;

		if constexpr(has_v<unversioned::nonmember::prologue, Archive, T>) {
			if (ar)
				prologue(ar, t);
		}

		if (ar) {
			ar.process(std::forward<T>(t));

			if constexpr (sizeof...(post_process) > 0) {
				if (ar)
					(void)((post_process(), ar) && ...);
			}

			if constexpr(has_v<unversioned::nonmember::epilogue, Archive, T>) {
				epilogue(ar, t);
			}
		}

		return ar;
	}

	template <typename Derived>
	class archive_output: public detail::archive_base<Derived>, public archive_is_output {
	protected:
		friend access;

		template <typename T>
		void process(T && t) {
			using must_process = trait::must_process_output<Derived, T>;

			if constexpr (!must_process::versioned) {
				if constexpr (must_process::member_serialize) {
					access::member_serialize(this->derived(), std::forward<T>(t));
				}
				else
				if constexpr (must_process::nonmember_serialize) {
					serialize(this->derived(), const_cast<std::decay_t<T>&>(t));
				}
				else
				if constexpr (must_process::member_save) {
					access::member_save(this->derived(), std::forward<T>(t));
				}
				else
				if constexpr (must_process::nonmember_save) {
					save(this->derived(), std::forward<T>(t));
				}
				else
				if constexpr (must_process::member_save_minimal) {
					access::process(this->derived(), access::member_save_minimal(this->derived(), std::forward<T>(t)));
				}
				else
				if constexpr (must_process::nonmember_save_minimal) {
					access::process(this->derived(), save_minimal(this->derived(), std::forward<T>(t)));
				}
				else
				if constexpr (must_process::member_save_minimal_with_error) {
					typename archive_output::error_t error{};
					const auto &v = access::member_save_minimal(this->derived(), std::forward<T>(t), error);
					access::process(this->derived(), v, [&]{
						this->error(error);
					});
				}
				else
				if constexpr (must_process::nonmember_save_minimal_with_error) {
					typename Derived::error_t error{};
					const auto &v = save_minimal(this->derived(), std::forward<T>(t), error);
					access::process(this->derived(), v, [&]{
						this->error(error);
					});
				}
				else {
					static_assert(util::delayed_assert<T>, "Internal error: serialization method requested not handled.");
				}
			}
			else
			if (auto ver = version_of_v<T>; this->process_version(t, ver)) {
				if constexpr (must_process::member_serialize) {
					access::member_serialize(this->derived(), std::forward<T>(t), ver);
				}
				else
				if constexpr (must_process::nonmember_serialize) {
					serialize(this->derived(), const_cast<std::decay_t<T>&>(t), ver);
				}
				else
				if constexpr (must_process::member_save) {
					access::member_save(this->derived(), std::forward<T>(t), ver);
				}
				else
				if constexpr (must_process::nonmember_save) {
					save(this->derived(), std::forward<T>(t), ver);
				}
				else
				if constexpr (must_process::member_save_minimal) {
					access::process(this->derived(), access::member_save_minimal(this->derived(), std::forward<T>(t), ver));
				}
				else
				if constexpr (must_process::nonmember_save_minimal) {
					access::process(this->derived(), save_minimal(this->derived(), std::forward<T>(t), ver));
				}
				else
				if constexpr (must_process::member_save_minimal_with_error) {
					typename archive_output::error_t error{};
					const auto &v = access::member_save_minimal(this->derived(), std::forward<T>(t), error, ver);
					access::process(this->derived(), v, [&]{
						this->error(error);
					});
				}
				else
				if constexpr (must_process::nonmember_save_minimal_with_error) {
					typename Derived::error_t error{};
					const auto &v = save_minimal(this->derived(), std::forward<T>(t), error, ver);
					access::process(this->derived(), v, [&]{
						this->error(error);
					});
				}
				else {
					static_assert(util::delayed_assert<T>, "Internal error: serialization method requested not handled.");
				}
			}
		}
	};


	template <typename Derived>
	class archive_input: public detail::archive_base<Derived>, public archive_is_input {
	protected:
		friend access;

		template <typename T>
		void process(T && t) {
			using namespace trait;
			using must_process = trait::must_process_input<Derived, T>;

			if constexpr (!must_process::versioned) {
				if constexpr (must_process::member_serialize) {
					access::member_serialize(this->derived(), std::forward<T>(t));
				}
				else
				if constexpr (must_process::nonmember_serialize) {
					serialize(this->derived(), const_cast<std::decay_t<T>&>(t));
				}
				else
				if constexpr (must_process::member_load) {
					access::member_load(this->derived(), std::forward<T>(t));
				}
				else
				if constexpr (must_process::nonmember_load) {
					load(this->derived(), std::forward<T>(t));
				}
				else
				if constexpr (must_process::member_load_minimal) {
					using output = trait::output_from_input_t<Derived>;
					typename has<unversioned::member::save_minimal, output, T>::type value{};

					if (access::process(this->derived(), value))
						access::member_load_minimal(this->derived(), std::forward<T>(t), std::move(value));
				}
				else
				if constexpr (must_process::nonmember_load_minimal) {
					using output = trait::output_from_input_t<Derived>;
					typename has<unversioned::nonmember::save_minimal, output, T>::type value{};

					if (access::process(this->derived(), value))
						//load_minimal(this->derived(), const_cast<std::decay_t<T>&>(t), std::move(value));
						load_minimal(this->derived(), std::forward<T>(t) /*const_cast<std::decay_t<T>&>(t)*/, std::move(value));
				}
				else
				if constexpr (must_process::member_load_minimal_with_error) {
					using output = trait::output_from_input_t<Derived>;
					typename trait::has_minimal_serialization<output, T>::type value;

					access::process(this->derived(), value, [&]{
						typename archive_input::error_t error{};
						access::member_load_minimal(this->derived(), std::forward<T>(t), std::move(value), error);
						this->error(error);
					});
				}
				else
				if constexpr (must_process::nonmember_load_minimal_with_error) {
					using output = trait::output_from_input_t<Derived>;
					typename trait::has_minimal_serialization<output, T>::type value;

					access::process(this->derived(), value, [&]{
						typename archive_input::error_t error{};
						load_minimal(this->derived(), std::forward<T>(t), value, error);
						this->error(error);
					});
				}
				else {
					static_assert(util::delayed_assert<T>, "Internal error: serialization method requested not handled.");
				}
			}
			else
			if (version_of_t<T> ver{}; this->process_version(t, ver)) {
				if constexpr (must_process::member_serialize) {
					access::member_serialize(this->derived(), std::forward<T>(t), ver);
				}
				else
				if constexpr (must_process::nonmember_serialize) {
					serialize(this->derived(), const_cast<std::decay_t<T>&>(t), ver);
				}
				else
				if constexpr (must_process::member_load) {
					access::member_load(this->derived(), std::forward<T>(t), ver);
				}
				else
				if constexpr (must_process::nonmember_load) {
					load(this->derived(), std::forward<T>(t), ver);
				}
				else
				if constexpr (must_process::member_load_minimal) {
					using output = trait::output_from_input_t<Derived>;
					typename has<versioned::member::save_minimal, output, T>::type value;

					if (access::process(this->derived(), value))
						access::member_load_minimal(this->derived(), std::forward<T>(t), std::move(value), ver);
				}
				else
				if constexpr (must_process::nonmember_load_minimal) {
					using output = trait::output_from_input_t<Derived>;
					typename has<versioned::nonmember::save_minimal, output, T>::type value;

					if (access::process(this->derived(), value))
						load_minimal(this->derived(), std::forward<T>(t), std::move(value), ver);
				}
				else
				if constexpr (must_process::member_load_minimal_with_error) {
					using output = trait::output_from_input_t<Derived>;
					typename trait::has_minimal_serialization<output, T>::type value;

					access::process(this->derived(), value, [&]{
						typename archive_input::error_t error{};
						access::member_load_minimal(this->derived(), std::forward<T>(t), std::move(value), error, ver);
						this->error(error);
					});
				}
				else
				if constexpr (must_process::nonmember_load_minimal_with_error) {
					using output = trait::output_from_input_t<Derived>;
					typename trait::has_minimal_serialization<output, T>::type value;

					access::process(this->derived(), value, [&]{
						typename archive_input::error_t error{};
						load_minimal(this->derived(), std::forward<T>(t), value, error, ver);
						this->error(error);
					});
				}
				else {
					static_assert(util::delayed_assert<T>, "Internal error: serialization method requested not handled.");
				}
			}
		}
	};

}

#include "../serialization/types/common.hpp"

#endif // FZ_SERIALIZATION_SERIALIZATION_HPP
