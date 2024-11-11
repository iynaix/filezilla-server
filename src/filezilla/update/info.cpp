#include <libfilezilla/signature.hpp>

#include "../logger/modularized.hpp"

#include "info.hpp"
#include "../sys_info.hpp"

namespace fz::update {

info::info()
{}

info::operator std::string() const
{
	if (data_)
		return data_->operator std::string();

	return {};
}

info::operator std::wstring() const
{
	if (data_)
		return data_->operator std::wstring();

	return {};
}

info::info(std::string_view raw_data, build_info::flavour_type flavour, allow allowed, fz::logger_interface &logger_)
	// Default to invalid values
	: flavour_()
{
	logger::modularized logger(logger_, "Update Info");

	const auto pub_key = fz::public_verification_key::from_base64([&flavour] {
		if (flavour == fz::build_info::flavour_type::standard)
			return "XUnB/QI88+Yx5PeCmq4pGyTUs6CBSzt/ARRpnKWV4P8=";

		if (flavour == fz::build_info::flavour_type::professional_enterprise)
			return "yvPHkqRQRPcejQyhlFzYEmgmjJppL1kIJsdf+79MGho=";

		return "";
	}());

	auto verify_signature = [&pub_key](const std::string_view &message, const std::vector<std::string_view> &tokens) -> std::size_t
	{
		if (message.empty())
			return 0;

		for (std::size_t i = 1; i < tokens.size(); ++i) {
			auto &t = tokens[i];

			if (t.substr(0, 4) != "sig:")
				continue;

			const auto &sig = fz::base64_decode_s(fz::to_utf8(t.substr(4)));

			if (!sig.empty() && fz::verify(message, sig, pub_key))
				return i;
		}

		return 0;
	};

	enum {
		got_nothing,
		got_version,
		got_release,
		got_beta,
		got_eol
	} status{};

	std::string_view changelog;

	data_.emplace();

	for (auto l: strtokenizer(raw_data, "\n", false)) {
		if (l.empty()) {
			changelog = {l.data() + 1, raw_data.size() - std::size_t(l.data() - raw_data.data()) - 1};
			fz::trim(changelog);
			break;
		}

		auto tokens = strtok_view(l, ' ');

		if (tokens.size() > 2 && (tokens[0] == "version")) {
			if (status > got_version)
				continue;

			if (auto i = verify_signature(tokens[1], tokens)) {
				status = got_version;

				data_->version = tokens[1];
				data_->signature = tokens[i];
			}

			continue;
		}

		if (tokens.size() > 6) {
			auto new_status = [&] {
				if (tokens[0] == "release" && allowed >= allow::release && status <= got_release)
					return got_release;

				if (tokens[0] == "beta" && allowed >= allow::beta && status <= got_beta)
					return got_beta;

				return got_nothing;
			}();

			if (new_status == got_nothing)
				continue;

			if (auto size = fz::to_integral<int64_t>(tokens[3]); size <= 0)
				logger.log(logmsg::debug_warning, L"Invalid file size");
			else
			if (tokens[4] != "sha512")
				logger.log(logmsg::debug_warning, L"Invalid SHA type");
			else {
				auto raw_hash = fz::hex_decode<std::string>(tokens[5]);
				raw_hash += '\0';
				raw_hash += tokens[1];

				if (auto i = verify_signature(raw_hash, tokens)) {
					status = new_status;

					data_->version = tokens[1];
					data_->file.emplace();
					data_->file->url = tokens[2];
					data_->file->size = size;
					data_->file->hash = tokens[5];
					data_->file->type = status == got_beta ? allow::beta : allow::release;
					data_->signature = tokens[i];
				}
			}

			continue;
		}

		if (tokens.size() > 1 && (tokens[0] == "eol")) {
			if (status == got_eol)
				continue;

			auto data = std::string(fz::build_info::host);
			data += '|';
			data += fz::build_info::version;
			data += '|';
			data += fz::sys_info::os_version;

			if (auto i = verify_signature(data, tokens)) {
				status = got_eol;

				data_->version = {};
				data_->file.reset();
				data_->signature = tokens[i];
			}

			continue;
		}
	}

	if (status == got_nothing)
		data_.reset();
	else {
		data_->changelog = changelog;
		flavour_ = flavour;
	}
}

template <typename String>
static String toString(const info::data &data)
{
	using C = typename String::value_type;
	String result;

	if (!data.version) {
		// EOL
		result = fz::sprintf(fzS(C, "eol %s\n"), data.signature);
	}
	else
	if (!data.file) {
		// version
		result = fz::sprintf(fzS(C, "version %s %s\n"), data.version, data.signature);
	}
	else {
		// release or beta
		result = fz::sprintf(fzS(C, "%s %s %s %d sha512 %s %s\n"), data.file->type, data.version, data.file->url, data.file->size, data.file->hash, data.signature);
	}

	if (!data.changelog.empty()) {
		result += fzS(C, "\n");
		result += fz::toString<String>(data.changelog);
	}

	return result;
}

info::data::operator std::string() const
{
	return toString<std::string>(*this);
}

info::data::operator std::wstring() const
{
	return toString<std::wstring>(*this);
}

info::retriever::~retriever()
{
}

info info::retriever::make_info(std::string_view raw_data, allow allowed, logger_interface &logger)
{
	return {raw_data, fz::build_info::flavour, allowed, logger};
}

info::raw_data_retriever::~raw_data_retriever()
{
}

}
