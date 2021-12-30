#include "thumbnail.hh"

#include "ByteArray.hpp"

#include <QImageReader>

#include <zstd.h>

namespace cornus {

Thumbnail* LoadThumbnail(const QString &full_path, const u64 &file_id,
	const QByteArray &ext, const int icon_w, const int icon_h,
	const TabId tab_id, const DirId dir_id)
{
	QImageReader reader = QImageReader(full_path, ext);
	reader.setScaledSize(QSize(icon_w, icon_h));
	QImage img = reader.read();
	if (img.isNull())
	{
		//mtl_info("Failed: %s", qPrintable(full_path));
		return nullptr;
	}
	auto *thumbnail = new Thumbnail();
	thumbnail->img = img;
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
			int status = global_data->CondWaitForWorkQueueChange();
			if (status != 0)
			{
				mtl_status(status);
				break;
			}
		}
		
		if (work_queue.isEmpty())
			continue;
		
		for (ThumbLoaderData *th_data: global_data->threads)
		{
			if (!th_data->TryLock()) // IT IS LOCKED!
				continue;
			if (th_data->new_work != nullptr)
			{
				th_data->Unlock();
				continue;
			}
			const int last_one = work_queue.size() - 1;
			{
				th_data->new_work = work_queue[last_one];
				th_data->SignalNewWorkAvailable();
				th_data->Unlock();
			}
			work_queue.remove(last_one);
			
			if (work_queue.isEmpty())
				break;
		}
	}
	
	global_data->Unlock();
	return nullptr;
}

static void FreeThumbnailMemory(void *data)
{
	uchar *m = (uchar*)data;
	delete[] m;
}

namespace thumbnail {
QImage ImageFromByteArray(ByteArray &ba)
{
	if (ba.size() <= ThumbnailHeaderSize)
		return QImage();
	
	const int w = ba.next_i32();
	const int h = ba.next_i32();
	const int bpl = ba.next_i32();
	const auto format = static_cast<QImage::Format>(ba.next_i32());
	ba.to(0);
	char *src_data = (ba.data() + ThumbnailHeaderSize);
	const i64 buflen = ba.size() - ThumbnailHeaderSize;
	
	const i64 frame_size = ZSTD_getFrameContentSize(src_data, buflen);
	uchar *frame_buf = new uchar[frame_size];
	const i64 dec_size = ZSTD_decompress(frame_buf, frame_size, src_data, buflen);
	Q_UNUSED(dec_size);
	
	return QImage(frame_buf, w, h, bpl, format, FreeThumbnailMemory);
}
}
}
