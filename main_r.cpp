#include <QCoreApplication>
#include <QCryptographicHash>

#include "ByteArray.hpp"
#include "decl.hxx"
#include "err.hpp"
#include "str.hxx"
#include "io/io.hh"
#include "io/Task.hpp"

namespace cornus {

struct Args {
	cornus::ByteArray ba;
	int client_fd = -1;
	IOAction default_action = IOAction::None;
};

bool CheckSecret(const u64 n, const QString &real_hash_str)
{
	const QByteArray ba = QByteArray::number(qulonglong(n));
	const QByteArray hash_ba = QCryptographicHash::hash(ba, QCryptographicHash::Md5);
	const QString hash_str = QString(hash_ba.toHex());
	return hash_str == real_hash_str;
}

void* ProcessRequest(void *p)
{
	pthread_detach(pthread_self());
	Args *args = (Args*)p;
	close(args->client_fd);
	
	auto &ba = args->ba;
	ba.to(0);
	ba.next_u64(); // skip secret num
	const auto msg = ba.next_u32() & ~(7u << 29);
	const bool paste_links = (msg == (io::MessageType)io::Message::PasteLinks);
	if (paste_links || (msg == (io::MessageType)io::Message::PasteRelativeLinks))
	{
		QString to_dir_path = ba.next_string();
		QVector<QString> paths;
		while (ba.has_more()) {
			paths.append(ba.next_string());
		}
		
		if (paste_links) {
			io::PasteLinks(paths, to_dir_path, nullptr);
		} else {
			io::PasteRelativeLinks(paths, to_dir_path, nullptr);
		}
	} else if (msg == (io::MessageType)io::Message::RenameFile) {
		const auto old_path = ba.next_string().toLocal8Bit();
		const auto new_path = ba.next_string().toLocal8Bit();
		::rename(old_path.data(), new_path.data());
	} else {
		ba.to(0);
		io::Task *task = io::Task::From(args->ba, HasSecret::Yes);
		task->SetDefaultAction(args->default_action);
		task->StartIO();
		delete task;
	}
	
	delete args;
	return nullptr;
}

void Listen(const QString &hash_str, const IOAction io_action)
{
	int daemon_sock_fd = cornus::io::socket::Daemon(cornus::RootSocketPath);
	if (daemon_sock_fd == -1) {
		mtl_info("Another root daemon is running. Exiting.");
		exit(0);
	}
	
	pthread_t th;
	int failed_count = 0;
	// secret_num=u64, msg_type=u32
	const isize min_size = sizeof(u64) + sizeof(u32);
	
	while (failed_count < 5)
	{
		const int client_fd = accept(daemon_sock_fd, NULL, NULL);
		if (client_fd == -1)
		{
			mtl_status(errno);
			break;
		}
		
		Args *args = new Args();
		ByteArray &ba = args->ba;
		
		if (!ba.Receive(client_fd, cornus::CloseSocket::No)) {
			close(client_fd);
			delete args;
			continue;
		}
		
		if (ba.size() < min_size || !CheckSecret(ba.next_u64(), hash_str))
		{
			failed_count++;
			close(client_fd);
			delete args;
			continue;
		}
		
		const auto msg = ba.next_u32() & ~(7u << 29);
		
		if (msg == (io::MessageType)io::Message::CheckAlive) {
			close(client_fd);
			delete args;
			continue;
		}
		
		args->client_fd = client_fd;
		args->default_action = io_action;
		int status = pthread_create(&th, NULL, cornus::ProcessRequest, args);
		if (status != 0) {
			mtl_status(status);
			break;
		}
		
		break;
	}
	
	close(daemon_sock_fd);
}

void PrintHelp(const char *exec_name)
{
	const char *s = "is a helper executable of \"cornus\"."
	" It is used to do IO " MTL_UNDERLINE_START MTL_BOLD_START
	"with root privileges" MTL_BOLD_END MTL_UNDERLINE_END
	", which is why it's launched by \"cornus\" in rare situations"
	" and the process exits as soon as the requested IO is done.";
	printf("%s %s\n", exec_name, s);
}
} // cornus::

int main(int argc, char *argv[])
{
	QCoreApplication app(argc, argv);
	
	if (argc <= 1) {
		cornus::PrintHelp(argv[0]);
		return 0;
	}
	
	const QString hash_str = argv[1];
	const QString action_str = argv[2];
	bool ok;
	int action_i = action_str.toInt(&ok);
	if (ok)
		cornus::Listen(hash_str, static_cast<cornus::IOAction>(action_i));
	
	return 0;
}

