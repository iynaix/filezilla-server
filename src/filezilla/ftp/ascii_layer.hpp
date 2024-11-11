#ifndef FTP_ASCII_LAYER_HPP
#define FTP_ASCII_LAYER_HPP

#include <optional>

#include <libfilezilla/socket.hpp>
#include <libfilezilla/buffer.hpp>

namespace fz::ftp {

class ascii_layer: public socket_layer
{
public:
	ascii_layer(event_handler* handler, socket_interface& next_layer);

	int read(void *data, unsigned int size, int &error) override;
	int write(const void *data, unsigned int size, int &error) override;

	int connect(const native_string &host, unsigned int port, address_type family) override;
	socket_state get_state() const override;
	int shutdown() override;

private:
	std::optional<char> tmp_read_{};
	unsigned char last_written_ch_{};
	buffer tmp_write_{};
	std::vector<int> crpos_{};
};

}

#endif // EOL_TRANSFORM_LAYER_HPP
