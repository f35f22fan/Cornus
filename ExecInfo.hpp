#pragma once

#include "types.hxx"

#include <QString>

#include <fcntl.h>
#include <sys/stat.h>

namespace cornus {

namespace ExecType {
	const u2 None = 0;
	const u2 Elf = 1u << 0;
	const u2 ShellScript = 1u << 1;
	const u2 BatScript = 1u << 2;
}

class ExecInfo {
public:
	ExecInfo();
	virtual ~ExecInfo();
	
	bool Run(const QString &app_path, const QString &dir_path) const;

	bool is_elf() const { return type & ExecType::Elf; }
	bool is_shell_script() const { return type & ExecType::ShellScript; }
	bool has_exec_bit() const { return mode & (S_IXUSR|S_IXGRP|S_IXOTH); }
	bool is_regular_file() const { return S_ISREG(mode); }
	bool is_symlink() const { return S_ISLNK(mode); }
	
	mode_t mode = 0;
	u2 type = 0;
	QString starter;
	
private:
	
};
}
