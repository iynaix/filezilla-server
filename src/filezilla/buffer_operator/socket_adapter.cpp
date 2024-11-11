#include "socket_adapter.hpp"

namespace {

struct shutdown_event_tag{};
using shutdown_event = fz::simple_event<shutdown_event_tag>;

}

fz::buffer_operator::socket_adapter::socket_adapter(fz::event_handler &target_handler, std::size_t max_readable_amount, std::size_t max_writable_amount_left_before_reading_again)
	: event_handler(target_handler.event_loop_)
	, max_readable_amount_(max_readable_amount)
	, max_writable_amount_left_before_reading_again_(max_writable_amount_left_before_reading_again)
{}

fz::buffer_operator::socket_adapter::~socket_adapter() {
	remove_handler();
}

void fz::buffer_operator::socket_adapter::set_socket(fz::socket_interface *si) {
	if (si_ != si) {
		if (si_)
			si_->set_event_handler(nullptr);

		si_ = si;

		if (si) {
			si->set_event_handler(this);
			waiting_for_read_event_ = true;
			waiting_for_write_event_ = true;
		}
	}
}

fz::socket_interface *fz::buffer_operator::socket_adapter::get_socket() const {
	return si_;
}

void fz::buffer_operator::socket_adapter::set_max_readable_amount(std::size_t max)
{
	max_readable_amount_ = max;
}

void fz::buffer_operator::socket_adapter::shutdown(int err) {
	shutdown_err_ = err ? err : -ESHUTDOWN;
	shutting_down_ = true;

	event_handler::send_event<shutdown_event>();
}

int fz::buffer_operator::socket_adapter::consume_buffer() {
	if (!si_ || waiting_for_write_event_) {
		return EAGAIN;
	}

	auto buffer = consumer::get_buffer();
	if (!buffer) {
		return EFAULT;
	}

	int error = 0;
	auto written = si_->write(buffer->get(), static_cast<unsigned int>(buffer->size()), error);

	if (written >= 0) {
		buffer->consume(size_t(written));

		if (shutting_down_) {
			on_shutdown_event();
		}
		else
		if (adder_waiting_for_consumable_amount_to_be_low_enough_ && buffer->size() <= max_writable_amount_left_before_reading_again_) {
			adder_waiting_for_consumable_amount_to_be_low_enough_ = false;
			adder::send_event(0);
		}
	}

	if (error == EAGAIN)
		waiting_for_write_event_ = true;

	return error;
}

int fz::buffer_operator::socket_adapter::add_to_buffer() {
	if (!si_ || waiting_for_read_event_ || adder_waiting_for_consumable_amount_to_be_low_enough_) {
		return EAGAIN;
	}

	if (max_writable_amount_left_before_reading_again_ < std::numeric_limits<std::size_t>::max()) {
		auto con_buffer = consumer::get_buffer();
		if (!con_buffer) {
			return EFAULT;
		}

		if (con_buffer->size() > max_writable_amount_left_before_reading_again_) {
			adder_waiting_for_consumable_amount_to_be_low_enough_ = true;
			return EAGAIN;
		}
	}

	auto buffer = adder::get_buffer();
	if (!buffer)
		return EINVAL;

	int error = 0;
	auto amount_to_read = max_readable_amount_-buffer->size();

	if (amount_to_read > 0) {
		int read = si_->read(buffer->get(amount_to_read), static_cast<unsigned int>(amount_to_read), error);

		if (read > 0)
			buffer->add(size_t(read));
		else
		if (read == 0)
			error = ENODATA;
	}
	else
		error = ENOBUFS;

	if (error == EAGAIN)
		waiting_for_read_event_ = true;

	return error;
}

void fz::buffer_operator::socket_adapter::operator()(const fz::event_base &ev) {
	fz::dispatch<
		fz::socket_event,
		shutdown_event
	>(ev, this,
		&socket_adapter::on_socket_event,
		&socket_adapter::on_shutdown_event
	);
}

void fz::buffer_operator::socket_adapter::on_socket_event(fz::socket_event_source *, fz::socket_event_flag type, int error)
{
	switch (type) {
		case fz::socket_event_flag::read:
			waiting_for_read_event_ = false;
			if (!shutting_down_)
				adder::send_event(error);
			break;

		case fz::socket_event_flag::write:
		case fz::socket_event_flag::connection: // connection events double as write events
			waiting_for_write_event_ = false;
			if (si_->get_state() == socket_state::shutting_down)
				on_shutdown_event();
			else
				consumer::send_event(error);
			break;

		case fz::socket_event_flag::connection_next:
			assert(type != fz::socket_event_flag::connection_next);
			break;
	}
}

void fz::buffer_operator::socket_adapter::on_shutdown_event()
{
	auto buffer = consumer::get_buffer();

	if (!si_ || !buffer || (buffer->size() == 0 && si_->shutdown() != EAGAIN)) {
		consumer::send_event(shutdown_err_);
		shutting_down_ = false;
		shutdown_err_ = 0;
	}
}
