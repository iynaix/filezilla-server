if ENABLE_FZ_WEBUI
  MAYBE_WEBUI = webui/app
endif

SUBDIRS = . $(MAYBE_WEBUI)

noinst_LIBRARIES = libfilezilla-common.a

noinst_HEADERS = \
	acme/cert_info.hpp \
	acme/challenges.hpp \
	acme/client.hpp \
	acme/client/challenger.hpp \
	acme/daemon.hpp \
	authentication/authenticator.hpp \
	authentication/autobanner.hpp \
	authentication/credentials.hpp \
	authentication/error.hpp \
	authentication/file_based_authenticator.hpp \
	authentication/method.hpp \
	authentication/password.hpp \
	authentication/password_with_impersonation.hpp \
	authentication/sqlite_token_db.hpp \
	authentication/throttled_authenticator.hpp \
	authentication/token_manager.hpp \
	authentication/user.hpp \
	badge.hpp \
	build_info.hpp \
	covariant.hpp \
	debug.hpp \
	enum_bitops.hpp \
	event_loop_pool.hpp \
	expected.hpp \
	http/body_chunker.hpp \
	http/client.hpp \
	http/field.hpp \
	http/handlers/authorizator.hpp \
	http/handlers/authorizator/authorization.hpp \
	http/handlers/authorized_file_server.hpp \
	http/handlers/authorized_file_sharer.hpp \
	http/handlers/file_server.hpp \
	http/handlers/router.hpp \
	http/headers.hpp \
	http/message_consumer.hpp \
	http/request.hpp \
	http/response.hpp \
	http/server.hpp \
	http/server/request.hpp \
	http/server/responder.hpp \
	http/server/session.hpp \
	http/server/session/transaction.hpp \
	http/server/transaction.hpp \
	impersonator/archives.hpp \
	impersonator/channel.hpp \
	impersonator/client.hpp \
	impersonator/messages.hpp \
	impersonator/parent_proxy.hpp \
	impersonator/process.hpp \
	impersonator/server.hpp \
	impersonator/util.hpp \
	intrusive_list.hpp \
	known_paths.hpp \
	logger/file.hpp \
	logger/hierarchical.hpp \
	logger/modularized.hpp \
	logger/null.hpp \
	logger/scoped.hpp \
	logger/splitter.hpp \
	logger/stdio.hpp \
	logger/type.hpp \
	mpl/append.hpp \
	mpl/arity.hpp \
	mpl/at.hpp \
	mpl/contains.hpp \
	mpl/count.hpp \
	mpl/count_if.hpp \
	mpl/fold.hpp \
	mpl/for_each.hpp \
	mpl/identity.hpp \
	mpl/if.hpp \
	mpl/index_of.hpp \
	mpl/lambda.hpp \
	mpl/next.hpp \
	mpl/placeholders.hpp \
	mpl/prepend.hpp \
	mpl/remove.hpp \
	mpl/rename.hpp \
	mpl/size.hpp \
	mpl/size_t.hpp \
	mpl/vector.hpp \
	mpl/with_index.hpp \
	port_randomizer.hpp \
	preprocessor/cat.hpp \
	preprocessor/expand.hpp \
	preprocessor/identity.hpp \
	preprocessor/str.hpp \
	receiver.hpp \
	receiver/async.hpp \
	receiver/context.hpp \
	receiver/detail.hpp \
	receiver/enabled_for_receiving.hpp \
	receiver/event.hpp \
	receiver/glue/rmp.hpp \
	receiver/handle.hpp \
	receiver/interface.hpp \
	receiver/sync.hpp \
	remove_event.hpp \
	rmp/address_info.hpp \
	rmp/any_exception.hpp \
	rmp/any_message.hpp \
	rmp/command.hpp \
	rmp/dispatch.hpp \
	rmp/engine.hpp \
	rmp/engine/access.hpp \
	rmp/engine/client.hpp \
	rmp/engine/client.ipp \
	rmp/engine/dispatcher.hpp \
	rmp/engine/server.hpp \
	rmp/engine/server.ipp \
	rmp/engine/session.hpp \
	rmp/engine/session.ipp \
	rmp/engine/visitor.hpp \
	rmp/exceptions.hpp \
	rmp/exceptions/generic.hpp \
	rmp/exceptions/message_not_implemented.hpp \
	rmp/exceptions/serialization_error.hpp \
	rmp/glue/receiver.hpp \
	rmp/version.hpp \
	serialization/external/pugixml/pugiconfig.hpp \
	serialization/external/pugixml/pugixml.hpp \
	serialization/external/pugixml/pugixml.cpp \
	serialization/types/acme.hpp \
	serialization/types/autobanner.hpp \
	serialization/types/expected.hpp \
	serialization/types/fs_path.hpp \
	serialization/types/ftp_server_options.hpp \
	serialization/types/json.hpp \
	serialization/types/local_filesys.hpp \
	serialization/types/logger_file_options.hpp \
	serialization/types/network_interface.hpp \
	serialization/types/securable_socket_cert_info.hpp \
	serialization/types/tvfs.hpp \
	serialization/types/update.hpp \
	serialization/types/webui_server_options.hpp \
	serialization/version.hpp \
	shared_context.hpp \
	socket_stack.hpp \
	string.hpp \
	strresult.hpp \
	strsyserror.hpp \
	sys_info.hpp \
	tcp/client.hpp \
	tcp/proxy_layer.hpp \
	tcp/server.hpp \
	tcp/session.hpp \
	tls_exit.hpp \
	transformed_view.hpp \
	tvfs/backend.hpp \
	tvfs/backends/local_filesys.hpp \
	tvfs/engine.hpp \
	tvfs/entry.hpp \
	tvfs/events.hpp \
	tvfs/limits.hpp \
	tvfs/mount.hpp \
	tvfs/permissions.hpp \
	tvfs/placeholders.hpp \
	tvfs/validation.hpp \
	update/checker.hpp \
	update/info.hpp \
	update/info_retriever/chain.hpp \
	update/info_retriever/null.hpp \
	update/raw_data_retriever/http.hpp \
	util/bits.hpp \
	util/copies_counter.hpp \
	util/demangle.hpp \
	util/dispatcher.hpp \
	util/filesystem.hpp \
	rmp/message.hpp \
	serialization/access.hpp \
	serialization/archives/argv.hpp \
	serialization/archives/binary.hpp \
	serialization/archives/fwd.hpp \
	serialization/archives/xml.hpp \
	serialization/detail/base.hpp \
	serialization/helpers.hpp \
	serialization/serialization.hpp \
	serialization/trait.hpp \
	serialization/types/binary_address_list.hpp \
	serialization/types/common.hpp \
	serialization/types/containers.hpp \
	serialization/types/hostaddress.hpp \
	tcp/address_info.hpp \
	tcp/address_list.hpp \
	tcp/automatically_serializable_binary_address_list.hpp \
	tcp/binary_address_list.hpp \
	tcp/listener.hpp \
	tcp/temporary_address_list.hpp \
	util/buffer_streamer.hpp \
	util/integral_ops.hpp \
	util/invoke_later.hpp \
	util/io.hpp \
	util/locking_wrapper.hpp \
	util/options.hpp \
	util/overload.hpp \
	util/parser.hpp \
	util/proof_of_work.hpp \
	util/scope_guard.hpp \
	util/serializable.hpp \
	util/thread_id.hpp \
	util/tools.hpp \
	util/traits.hpp \
	util/tuple_insert.hpp \
	util/tuple_slice.hpp \
	util/typemask.hpp \
	util/vector_map.hpp \
	util/welcome_message.hpp \
	util/xml_archiver.hpp \
	channel.hpp \
	securable_socket.hpp \
	hostaddress.hpp \
	ftp/session.hpp \
	ftp/server.hpp \
	ftp/ascii_layer.hpp \
	ftp/controller.hpp \
	ftp/commander.hpp \
	serialization/types/tuple.hpp \
	serialization/types/variant.hpp \
	serialization/types/optional.hpp \
	serialization/types/time.hpp \
	buffer_operator/detail/base.hpp \
	buffer_operator/adder.hpp \
	buffer_operator/consumer.hpp \
	buffer_operator/file_reader.hpp \
	buffer_operator/file_writer.hpp \
	buffer_operator/serialized_adder.hpp \
	buffer_operator/serialized_consumer.hpp \
	buffer_operator/streamed_adder.hpp \
	buffer_operator/line_consumer.hpp \
	buffer_operator/tvfs_entries_lister.hpp \
	buffer_operator/socket_adapter.hpp \
	pipe.hpp \
	buffer_operator/monitored_adder.hpp \
	buffer_operator/monitored_consumer.hpp \
	buffer_operator/detail/monitored_base.hpp \
	value_or.hpp \
	service.hpp \
	webui/rewriter.hpp \
	webui/server.hpp \
	webui/templated_index_wrapper.hpp

if !FZ_WINDOWS
noinst_HEADERS += \
	signal_notifier.hpp
endif

libfilezilla_common_a_SOURCES = \
	acme/cert_info.cpp \
	acme/client.cpp \
	acme/client/challenger.cpp \
	acme/daemon.cpp \
	authentication/authenticator.cpp \
	authentication/autobanner.cpp \
	authentication/credentials.cpp \
	authentication/file_based_authenticator.cpp \
	authentication/method.cpp \
	authentication/password.cpp \
	authentication/password_with_impersonation.cpp \
	authentication/throttled_authenticator.cpp \
	authentication/token_manager.cpp \
	authentication/user.cpp \
	buffer_operator/socket_adapter.cpp \
	build_info.cpp \
	event_loop_pool.cpp \
	hostaddress.cpp \
	http/client.cpp \
	http/field.cpp \
	http/handlers/authorizator.cpp \
	http/handlers/authorizator/authorization.cpp \
	http/handlers/authorized_file_server.cpp \
	http/handlers/authorized_file_sharer.cpp \
	http/handlers/file_server.cpp \
	http/handlers/router.cpp \
	http/headers.cpp \
	http/message_consumer.cpp \
	http/response.cpp \
	http/server.cpp \
	http/server/request.cpp \
	http/server/session.cpp \
	http/server/session/transaction.cpp \
	impersonator/archives.cpp \
	impersonator/channel.cpp \
	impersonator/client.cpp \
	impersonator/parent_proxy.cpp \
	impersonator/process.cpp \
	impersonator/server.cpp \
	impersonator/util.cpp \
	logger/file.cpp \
	logger/hierarchical.cpp \
	logger/modularized.cpp \
	logger/null.cpp \
	logger/splitter.cpp \
	logger/stdio.cpp \
	port_randomizer.cpp \
	receiver/context.cpp \
	receiver/enabled_for_receiving.cpp \
	receiver/handle.cpp \
	securable_socket.cpp \
	channel.cpp \
	ftp/server.cpp \
	ftp/session.cpp \
	ftp/ascii_layer.cpp \
	ftp/commander.cpp \
	serialization/archives/argv.cpp \
	serialization/archives/xml.cpp \
	strresult.cpp \
	strsyserror.cpp \
	sys_info.cpp \
	tcp/client.cpp \
	tcp/listener.cpp \
	tcp/proxy_layer.cpp \
	tcp/server.cpp \
	tcp/session.cpp \
	tcp/binary_address_list.cpp \
	tcp/temporary_address_list.cpp \
	tcp/automatically_serializable_binary_address_list.cpp \
	pipe.cpp \
	tvfs/backend.cpp \
	tvfs/backends/local_filesys.cpp \
	tvfs/engine.cpp \
	tvfs/entry.cpp \
	tvfs/mount.cpp \
	tvfs/placeholders.cpp \
	tvfs/validation.cpp \
	update/checker.cpp \
	update/info.cpp \
	update/info_retriever/chain.cpp \
	update/info_retriever/null.cpp \
	update/raw_data_retriever/http.cpp \
	util/demangle.cpp \
	util/filesystem.cpp \
	util/invoke_later.cpp \
	util/io.cpp \
	util/proof_of_work.cpp \
	util/thread_id.cpp \
	util/tools.cpp \
	util/welcome_message.cpp \
	util/xml_archiver.cpp

if FZ_WINDOWS
    libfilezilla_common_a_SOURCES += \
        service/win32/service.cpp
else
    libfilezilla_common_a_SOURCES += \
        service/generic/service.cpp \
        signal_notifier.cpp
endif

if FZ_MAC
    libfilezilla_common_a_SOURCES += \
	    known_paths_osx.mm

    noinst_HEADERS += \
	    known_paths.cpp
else
    libfilezilla_common_a_SOURCES += \
	    known_paths.cpp
endif

ARFLAGS = cr

libfilezilla_common_a_CXXFLAGS = $(LIBFILEZILLA_CFLAGS) -fno-exceptions
libfilezilla_common_a_OBJCXXFLAGS = $(libfilezilla_common_a_CXXFLAGS)

if ENABLE_FZ_WEBUI
libfilezilla_common_a_SOURCES += \
	authentication/sqlite_token_db.cpp \
	webui/rewriter.cpp \
	webui/server.cpp \
	webui/templated_index_wrapper.cpp

libfilezilla_common_a_CXXFLAGS += $(LIBSQLITE3_CFLAGS)
endif


distclean-local:
	@rm -f build_datetime.hpp
