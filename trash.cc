#include "trash.hh"

#include "ByteArray.hpp"
#include "io/io.hh"

#include <QDir>
#include <QProcess>

namespace cornus::trash {

bool AddTrashNameToGitignore(const QString &new_path)
{
	ByteArray contents;
	CHECK_TRUE(io::ReadFile(new_path, contents));
	const QString trash_line = QLatin1String("\n")
		+ trash::basename_regex();
	QString new_data = contents.toString() + trash_line;
	auto ba = new_data.toLocal8Bit();
	auto ret = io::WriteToFile(new_path, ba.data(), ba.size());
	return (ret == io::Err::Ok);
}

const QString& basename()
{
	static const QString s = QLatin1String(".Trash_cornu$_");
	return s;
}

const QString& basename_regex()
{
	static const QString s = trash::basename() + QChar('*');
	return s;
}

QString CreateGlobalGitignore()
{
	QString new_path = QDir::homePath() + "/.gitignore_global";
	// git config --global core.excludesfile ~/.gitignore
	QProcess git;
	QStringList params;
	params << QLatin1String("config") << QLatin1String("--global")
	<< QLatin1String("core.excludesfile") << new_path;
	git.start(QLatin1String("git"), params);
	CHECK_TRUE_QSTR(git.waitForStarted());
	CHECK_TRUE_QSTR(git.waitForFinished());
	CHECK_TRUE_QSTR(io::EnsureRegularFile(new_path));
	CHECK_TRUE_QSTR(AddTrashNameToGitignore(new_path));
	
	return new_path;
}

void EmptyRecursively(const QString &dir_path)
{
	mtl_info();
}

QString EnsureTrashForFile(const QString &file_path)
{
	QString parent_dir = io::GetParentDirPath(file_path).toString();
	if (parent_dir.isEmpty())
		return QString();
	
	if (!parent_dir.endsWith('/'))
		parent_dir.append('/');
	
	QString ret;
	if(io::EnsureDir(parent_dir, trash::name(), &ret))
		return ret;
	return QString();
}

const QString& gitignore_global_path(const QString *override_data)
{
	static QString ret;
	
	if (override_data)
		ret = *override_data;
	else if (ret.isEmpty())
		ret = ReadGitignoreGlobal();
	
	return ret;
}

bool ListItems(const QString &dir_path, QMap<i64, QVector<Names> > &hash)
{
	QVector<QString> names;
	CHECK_TRUE((io::ListFileNames(dir_path, names) == io::Err::Ok));
	
	for (auto &name: names)
	{
		int index = name.indexOf('_');
		if (index < 1 || index == name.size() - 1)
			continue;
		
		QStringRef time_str = name.midRef(0, index);
		bool ok;
		i64 n = time_str.toLong(&ok, NumberBase);
		if (!ok)
			continue;
		
		Names pair = {name.mid(index + 1), name};
		
		if (hash.contains(n)) {
			hash[n].append(pair);
		} else {
			hash.insert(n, { pair });
		}
	}
	
	return true;
}

const QString& name()
{
	static const QString name = trash::basename() + QString::number(getuid());
	return name;
}

QString ReadGitignoreGlobal()
{
	QProcess git;
	QStringList params;
	params << QLatin1String("config") << QLatin1String("--get")
		<< QLatin1String("core.excludesfile");
	git.start(QLatin1String("git"), params);
	CHECK_TRUE_QSTR(git.waitForStarted());
	CHECK_TRUE_QSTR(git.waitForFinished());
	QByteArray result = git.readAll().trimmed();
	return result;
}

}