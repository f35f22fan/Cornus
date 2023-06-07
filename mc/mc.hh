#pragma once

#include <QString>

namespace cornus::mc {

QString ReadMkvTitle(QStringView full_path, bool *ok = 0);

}
