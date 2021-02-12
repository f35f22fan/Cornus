#include "desktop.hh"

#include "../ByteArray.hpp"

namespace cornus::io::desktop {

void SendOpenWithList(ByteArray &received_ba, const int fd)
{
	QString mime = received_ba.next_string();
	mtl_printq2("Received query with mime: ", mime);
	
	ByteArray ba;
	ba.add_string("some string");
	ba.Send(fd);
}

}
