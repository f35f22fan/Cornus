#pragma once

#include "../decl.hxx"
#include "decl.hxx"
#include "../io/decl.hxx"
#include "../err.hpp"

#include <QAbstractTableModel>
#include <QMouseEvent>
#include <QPoint>
#include <QTableView>

namespace cornus::gui {

class Table : public QTableView {
	Q_OBJECT
public:
	Table(TableModel *tm, App *app);
	virtual ~Table();
	
	TableModel* model() const { return model_; }
	virtual void dropEvent(QDropEvent *event) override;
	io::File* GetFileAtNTS(const QPoint &pos, const bool clone, int *ret_file_index = nullptr);
	io::File* GetFileAt(const int row); // returns cloned file
	int GetFirstSelectedFile(io::File **ret_cloned_file);
	QString GetFirstSelectedFileFullPath(QString *ext);
	int GetSelectedFilesCount();
	void ProcessAction(const QString &action);
	void ScrollToRow(int row);
	void ScrollToAndSelectRow(const int row, const bool deselect_others);
	void SelectAllFilesNTS(const bool flag, QVector<int> &indices);
	void SelectRowSimple(const int row, const bool deselect_others = false);
public slots:
	bool ScrollToAndSelect(QString full_path);
	
protected:
	virtual void dragEnterEvent(QDragEnterEvent *event) override;
	virtual void dragLeaveEvent(QDragLeaveEvent *event) override;
	virtual void dragMoveEvent(QDragMoveEvent *event) override;
	
	virtual void keyPressEvent(QKeyEvent *event) override;
	virtual void keyReleaseEvent(QKeyEvent *evt) override;
	virtual void mouseDoubleClickEvent(QMouseEvent *event) override;
	virtual void mouseMoveEvent(QMouseEvent *evt) override;
	virtual void mousePressEvent(QMouseEvent *event) override;
	
	virtual void paintEvent(QPaintEvent *event) override;
	virtual void resizeEvent(QResizeEvent *event) override;
private:
	NO_ASSIGN_COPY_MOVE(Table);
	
	void FinishDropOperation(QVector<io::File *> *files_vec, io::File *to_dir,
		Qt::DropAction drop_action);
	void HandleMouseSelection(QMouseEvent *evt, QVector<int> &indices);
	int IsOnFileNameStringNTS(const QPoint &pos, io::File **ret_file = nullptr);
	void ListSelectedFiles(QList<QUrl> &list);
	void SelectFileRangeNTS(const int row_start, const int row_end, QVector<int> &indices);
	int SelectNextRow(const int relative_offset, const bool deselect_all_others, QVector<int> &indices); // returns newly selected row
	void ShowRightClickMenu(const QPoint &pos);
	void SortingChanged(int logical, Qt::SortOrder order);
	
	App *app_ = nullptr;
	TableModel *model_ = nullptr;
	TableDelegate *delegate_ = nullptr;
	
	struct ShiftSelect {
		int starting_row = -1;
		bool do_restart = false;
		
	} shift_select_ = {};

	int drop_y_coord_ = -1;
	QPoint drag_start_pos_ = {-1, -1};
	
};

} // cornus::gui::
