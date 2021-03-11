#pragma once

#include <QTableView>

namespace cornus::gui {
class BasicTable: public QTableView {
public:

	virtual void UpdateColumnSizes() = 0;
private:

};
}
