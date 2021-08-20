#pragma once

#include "../err.hpp"

#include <QPainter>

namespace cornus::gui {
class RestorePainter {
public:
	RestorePainter(QPainter *p): p_(p) { p->save();}
	~RestorePainter() { p_->restore(); }
private:
	NO_ASSIGN_COPY_MOVE(RestorePainter);
	QPainter *p_ = nullptr;
};

}
