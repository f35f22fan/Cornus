#include "TasksWin.hpp"

#include "../MutexGuard.hpp"
#include "TaskGui.hpp"

#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QStyleOptionViewItem>

namespace cornus::gui {

TasksWin::TasksWin(QSystemTrayIcon *tray_icon) : pixmap_(32, 32), tray_icon_(tray_icon)
{
	default_icon_ = QIcon(cornus::AppIconPath);
	tray_icon_->setIcon(default_icon_);
	tray_icon_->setVisible(true);
	DetectThemeType();
	
	setFocusPolicy(Qt::WheelFocus);
	CreateGui();
	setWindowIcon(default_icon_);
	setWindowTitle("Cornus I/O");
	QList<QScreen*> screens = QGuiApplication::screens();
	if (!screens.isEmpty()) {
		screen_sz_ = screens[0]->availableSize();
	} else {
		screen_sz_ = QSize(1920, 1080);
	}
	
	win_w_ = std::max(800, screen_sz_.width() / 3);
	resize(win_w_, 100);
	progress_timer_ = new QTimer();
	connect(progress_timer_, &QTimer::timeout, this, &TasksWin::UpdateOverallProgress);
}

TasksWin::~TasksWin() {
	delete progress_timer_;
}

void TasksWin::add(cornus::io::Task *task)
{
	auto *task_gui = TaskGui::From(task, this);
	
	if (task_gui != nullptr) {
		tasks_.append(task_gui);
		layout_->addWidget(task_gui);
		adjustSize();
		
		if (!progress_timer_->isActive()) {
			progress_timer_->start(500);
		}
	}
}

void TasksWin::CreateGui()
{
	main_widget_ = new QWidget();
	setCentralWidget(main_widget_);
	layout_ = new QBoxLayout(QBoxLayout::TopToBottom);
	main_widget_->setLayout(layout_);
}


void TasksWin::DetectThemeType()
{
	const QStyleOptionViewItem option;
	const QColor c = option.palette.window().color();
	const i32 avg = (c.red() + c.green() + c.blue()) / 3;
	theme_type_ = (avg > 150) ? ThemeType::Light : ThemeType::Dark;
	//	mtl_info("avg: %d, light: %s", avg, (theme_type_ == ThemeType::Light)
	//		? "true" : "false");
}

void TasksWin::DrawPercent(cint percent) {
	QString msg = QString::number(percent);
	pixmap_.fill(QColor(0, 0, 0, 0));
	QPainter painter(&pixmap_);
	painter.setRenderHint(QPainter::Antialiasing);
	const QRect r(0, 0, pixmap_.width(), pixmap_.height());
	
	cint span_angle = (percent * 360 * 16) / 100;
	QColor arc_color = opposite_to_bg();
	arc_color.setAlpha(80);
	painter.setBrush(arc_color);
	painter.drawPie(r, 0, span_angle);
	
	QFont font = painter.font();
	cint pixelSize = r.width();
	font.setPixelSize(pixelSize * 0.9);
	font.setBold(false);
	painter.setFont(font);
	painter.setPen(green_color());
	painter.drawText(0, pixelSize * 0.75, msg);
	
	icon_ = pixmap_;
	tray_icon_->setIcon(icon_);
}

QSize TasksWin::minimumSizeHint() const { return sizeHint(); }

QSize TasksWin::sizeHint() const {
	cint count = layout_->count();
	if (count == 0)
		return QSize(0, 0);
	auto *item = layout_->itemAt(0);
	QSize sz = item->sizeHint();
	return QSize(win_w_, sz.height() * count);
}

QSize TasksWin::maximumSize() const {
	return QSize(win_w_, sizeHint().height());
}

void TasksWin::TaskDone(TaskGui *gui_task, const io::TaskState state)
{
	for (int i = 0; i < tasks_.size(); i++) {
		if (tasks_[i] == gui_task) {
			tasks_.removeAt(i);
			break;
		}
	}
	
	layout_->removeWidget(gui_task);
	delete gui_task;
	
	if (tasks_.isEmpty())
	{
		progress_timer_->stop();
		tray_icon_->setIcon(default_icon_);
		setVisible(false);
	}
	
	adjustSize();
}

void TasksWin::UpdateOverallProgress()
{
	if (layout_->count() == 0) {
		return;
	}
	i64 at_sum = 0, total_sum = 0, at = 0, total = 0;
	cint count = tasks_.size();
	io::Progress progress;
	for (int i = 0; i < count; i++) {
		TaskGui *tg = tasks_[i];
		io::Task *task = tg->task();
		task->GetShort(at, total);
		at_sum += at;
		total_sum += total;
	}
	
	ci64 percent = (total == 0) ? 0 : at_sum * 100 / total;
	DrawPercent(percent);
}

}
