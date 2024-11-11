#include "transaction.hpp"

namespace fz::http {

server::session::transaction::request &server::session::transaction::req()
{
	return request_;
}

fz::http::server::responder &server::session::transaction::res()
{
	return *this;
}

util::locked_proxy<server::session> server::session::transaction::get_session()
{
	mutex_.lock();
	return {s_, &mutex_};
}

event_loop &server::session::transaction::get_event_loop()
{
	return event_loop_;
}

server::session::transaction::transaction(event_loop &event_loop, session &s)
	: event_loop_(event_loop)
	, s_(&s)
	, request_(*this)
{
}

server::session::transaction::~transaction()
{
}

void server::session::transaction::detach()
{
	scoped_lock lock(mutex_);
	s_ = nullptr;

	std::visit([](auto &consumer) {
		consumer.set_buffer(nullptr);
		consumer.set_event_handler(nullptr);
	}, request_.body_writer_);

	std::visit([](auto &adder) {
		adder.set_buffer(nullptr);
		adder.set_event_handler(nullptr);
	}, response_.body_reader_);

	if (response_.body_chunker_) {
		response_.body_chunker_->set_buffer(nullptr);
		response_.body_chunker_->set_event_handler(nullptr);
	}
}

bool server::session::transaction::send_status(unsigned int code, std::string_view reason)
{
	if (auto s = get_session()) {
		return s->send_status(code, reason);
	}

	return false;
}

bool server::session::transaction::send_headers(std::initializer_list<std::pair<field::name_view, field::value_view>> list)
{
	if (auto s = get_session()) {
		return s->send_headers(list);
	}

	return false;
}

bool server::session::transaction::send_body(std::string_view body)
{
	if (auto s = get_session()) {
		return s->send_body(body);
	}

	return false;
}

bool server::session::transaction::send_body(tvfs::file_holder file)
{
	if (auto s = get_session()) {
		return s->send_body(std::move(file));
	}

	return false;
}

bool server::session::transaction::send_body(tvfs::entries_iterator it)
{
	if (auto s = get_session()) {
		return s->send_body(std::move(it));
	}

	return false;
}

bool server::session::transaction::send_end()
{
	if (auto s = get_session()) {
		return s->send_end();
	}

	return false;
}

void server::session::transaction::abort_send(std::string_view msg)
{
	if (auto s = get_session()) {
		s->abort_send(msg);
	}
}

server::session::transaction::request::~request()
{}

}
