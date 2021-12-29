#include "thumbnail.hh"

#include <QImageReader>

namespace cornus {

Thumbnail* LoadThumbnail(const QString &full_path, const u64 &file_id,
	const QByteArray &ext, const int icon_w, const int icon_h,
	const TabId tab_id, const DirId dir_id)
{
	QImageReader reader = QImageReader(full_path, ext);
	reader.setScaledSize(QSize(icon_w, icon_h));
	auto *thumbnail = new Thumbnail();
	thumbnail->img = reader.read();
	if (thumbnail->img.isNull())
	{
		delete thumbnail;
		return nullptr;
	}
	
	thumbnail->file_id = file_id;
	thumbnail->time_generated = time(NULL);
	thumbnail->w = icon_w;
	thumbnail->h = icon_h;
	thumbnail->tab_id = tab_id;
	thumbnail->dir_id = dir_id;
	
	return thumbnail;
}

void* GlobalThumbLoadMonitor(void *args)
{
	pthread_detach(pthread_self());
	GlobalThumbLoaderData *global_data = (GlobalThumbLoaderData*)args;
	CHECK_TRUE_NULL(global_data->Lock());
	
	while (!global_data->threads.isEmpty())
	{
		auto &work_queue = global_data->work_queue;
		if (work_queue.isEmpty())
		{
			int status = global_data->CondWait();
			if (status != 0)
			{
				mtl_status(status);
				break;
			}
		}
		
		if (work_queue.isEmpty())
			continue;
		
		int th_index = -1;
		for (ThumbLoaderData *th_data: global_data->threads)
		{
			th_index++;
			if (!th_data->TryLock()) // IT IS LOCKED!
				continue;
			if (th_data->new_work != nullptr)
			{
				th_data->Unlock();
				continue;
			}
			const int index = work_queue.size() - 1;
			{
				th_data->new_work = work_queue[index];
				th_data->SignalNewWorkAvailable();
				th_data->Unlock();
			}
			work_queue.remove(index);
			
			if (work_queue.isEmpty())
				break;
		}
	}
	
	global_data->Unlock();
	return nullptr;
}

}
