#include "ExecInfo.hpp"

#include "err.hpp"

#include <QProcess>
#include <QRegularExpression>

namespace cornus {

ExecInfo::ExecInfo() {}

ExecInfo::~ExecInfo() {}

bool ExecInfo::Run(const QString &app_path, const QString &dir_path) const
{
	QRegularExpression regex("[\\s]+");
	auto list = starter.splitRef(regex);
	if (list.isEmpty())
		return false;
	QString exe_path = list[0].trimmed().toString();
	if (exe_path.isEmpty())
		return false;
	QStringList args;
	
	for (int i = 1; i < list.size(); i++) {
		QStringRef next = list[i].trimmed();
		if (!next.isEmpty())
			args.append(next.toString());
	}
	
	args.append(app_path);
#ifdef CORNUS_DEBUG_EXECINFO
	mtl_printq2("exe_path: ", exe_path);
	for (const auto &next: args) {
		mtl_printq2("arg: ", next);
	}
	mtl_printq2("dir_path: ", dir_path);
#endif
	QProcess::startDetached(exe_path, args, dir_path);
	
	return true;
}

}


