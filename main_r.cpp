#include <QCoreApplication>

#include "err.hpp"
#include "str.hxx"

void PrintHelp(const char *exec_name)
{
	const char *s = "is a helper executable of \"cornus\"."
	" It is used to do IO " MTL_UNDERLINE_START MTL_BOLD_START
	"with root privileges" MTL_BOLD_END MTL_UNDERLINE_END
	", which is why it's launched by \"cornus\" in rare situations"
	" and the process exits as soon as the requested IO is done.";
	printf("%s %s\n", exec_name, s);
}

int main(int argc, char *argv[])
{
	QCoreApplication app(argc, argv);
	
	if (argc == 1) {
		PrintHelp(argv[0]);
		return 0;
	}
	
// pkexec env DISPLAY=$DISPLAY XAUTHORITY=$XAUTHORITY /home/fox/dev/Cornus/build/cornus
	
	auto list = app.arguments();
	
	for (auto &next: list)
	{
		mtl_info("arg: %s", qPrintable(next));
	}
	
	return 0;
}

