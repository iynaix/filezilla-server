#ifndef FZ_BUFFER_OPERATOR_SERIALIZED_ADDER_HPP
#define FZ_BUFFER_OPERATOR_SERIALIZED_ADDER_HPP

#include "../buffer_operator/adder.hpp"
#include "../serialization/archives/binary.hpp"

#if ENABLE_FZ_SERIALIZATION_DEBUG
#   include "../serialization/archives/xml.hpp"
#endif

namespace fz::buffer_operator {

	struct serialized_adder_data_base {};

	template <typename Buffer>
	struct serialized_adder_data;

	template <>
	struct serialized_adder_data<fz::buffer>: serialized_adder_data_base
	{
		serialized_adder_data() = default;

		explicit operator bool() const
		{
			return !err;
		}

	protected:
		friend class serialized_adder;
		fz::buffer buffer;
		int err = ENOMSG;
	};

	template <>
	struct serialized_adder_data<fz::buffer&>: serialized_adder_data_base
	{
		serialized_adder_data(fz::buffer &buffer)
			: buffer(buffer)
		{
		}

		explicit operator bool() const
		{
			return !err;
		}

		int error() const
		{
			return err && err != ENOMSG;
		}

	protected:
		friend class serialized_adder;
		fz::buffer &buffer;
		int err = ENOMSG;
	};

	template <>
	struct serialized_adder_data<void>: serialized_adder_data_base
	{
		serialized_adder_data()
			: buffer(get_thread_buffer())
		{
		}

		explicit operator bool() const
		{
			return !err;
		}

		int error() const
		{
			return err && err != ENOMSG;
		}

	protected:
		friend class serialized_adder;
		fz::buffer &buffer;
		int err = ENOMSG;

		static fz::buffer &get_thread_buffer()
		{
			thread_local fz::buffer buf;
			return buf;
		}
	};

	class serialized_adder: public adder {
		int add_to_buffer() override {
			auto buffer = get_buffer();
			if (!buffer)
				return EINVAL;

			if (event_sent_) {
				event_sent_ = false;
				return 0;
			}

			return EAGAIN;
		}

	private:
		std::size_t warning_buffer_size_;
		bool event_sent_{false};


	public:
		serialized_adder(std::size_t warning_buffer_size)
			: warning_buffer_size_{warning_buffer_size}
		{
		}

		template <typename T, typename... Ts>
		std::enable_if_t<(!std::is_base_of_v<serialized_adder_data_base, T> && ... && !std::is_base_of_v<serialized_adder_data_base, Ts>),int>
		serialize_to_buffer(const T &v, const Ts &... vs) {
			auto buffer = get_buffer();
			if (!buffer)
				return EINVAL;

			int err = serialize_to_buffer(*buffer, v, vs...);

			if (!err) {
				FZ_SERIALIZATION_DEBUG_LOG(L"serialize_to_buffer(%s): BS: %zu, WS: %zu", util::type_name(v), buffer->size(), warning_buffer_size_);

				if (!event_sent_)
					event_sent_ = send_event(0);

				if (buffer->size() > warning_buffer_size_)
					serialized_adder_buffer_overflow();
			}

			return err;
		}

		template <typename Buffer>
		int serialize_to_buffer(const serialized_adder_data<Buffer> &data) {
			if (data) {
				auto buffer = get_buffer();
				if (!buffer)
					return EINVAL;

				buffer->append(data.buffer.get(), data.buffer.size());

				FZ_SERIALIZATION_DEBUG_LOG(L"serialize_to_buffer(serialized_data): BS: %zu, WS: %zu", buffer->size(), warning_buffer_size_);

				if (!event_sent_)
					event_sent_ = send_event(0);

				if (buffer->size() > warning_buffer_size_)
					serialized_adder_buffer_overflow();
			}

			return data.err;
		}

		template <typename Buffer, typename T, typename... Ts>
		static int get_serialized_data(serialized_adder_data<Buffer> &data, const T &v, const Ts &... vs) {
			data.buffer.clear();
			return data.err = serialize_to_buffer(data.buffer, v, vs...);
		}

	private:
		template <typename T, typename... Ts>
		static int serialize_to_buffer(fz::buffer &buffer, const T &v, const Ts &... vs) {
			#if ENABLE_FZ_SERIALIZATION_DEBUG
				auto orig_size = buffer.size();
			#endif

			int err = serialization::binary_output_archive{buffer}(v, vs...).error();

			FZ_SERIALIZATION_DEBUG_LOG(L"BUFFER for [%s] (size: %zu, err: %d): [%s]", (util::type_name(v) + ... + (", " + util::type_name(vs))), buffer.size()-orig_size, err, fz::hex_encode<std::string>(std::string_view((const char *)buffer.get()+orig_size, buffer.size()-orig_size)));

			#if ENABLE_FZ_SERIALIZATION_DEBUG
				if (!err) {
					fz::buffer b;
					{
						serialization::xml_output_archive::buffer_saver saver(b);
						serialization::xml_output_archive xml(saver);
						xml(v, vs...);
					}

					FZ_SERIALIZATION_DEBUG_LOG(to_wstring(std::string_view(reinterpret_cast<const char *>(b.get()), b.size())));
				}
				else
					FZ_SERIALIZATION_DEBUG_LOG(L"Failed serialization of %s", util::type_name(v));
			#endif

			return err;
		}

		virtual void serialized_adder_buffer_overflow()
		{}
	};

}

#endif // FZ_BUFFER_OPERATOR_SERIALIZED_ADDER_HPP
