#include "thumbnail.hh"

#include "ByteArray.hpp"

#include <QImageReader>

#include <zstd.h>

namespace cornus {

Thumbnail* LoadThumbnail(const QString &full_path, const u64 &file_id,
	const QByteArray &ext, const int max_img_w, const int max_img_h,
	const TabId tab_id, const DirId dir_id)
{
	QImageReader reader = QImageReader(full_path, ext);
	auto sz = reader.size();
	if (sz.isEmpty())
	{
	//	mtl_printq(full_path);
		sz = QSize(max_img_w, max_img_h);
	}
	
	const double pw = sz.width();
	const double ph = sz.height();
	double used_w, used_h;
	if (pw > max_img_w || ph > max_img_h)
	{
		double w_ratio = pw / max_img_w;
		double h_ratio = ph / max_img_h;
		const double ratio = std::max(w_ratio, h_ratio);
		used_w = pw / ratio;
		used_h = ph / ratio;
	} else {
		used_w = pw;
		used_h = ph;
	}
	
	reader.setScaledSize(QSize(used_w, used_h));
	
	QImage img = reader.read();
	if (img.isNull())
	{
		return nullptr;
	}
	
//	mtl_info("%s max: %d/%d, loaded: %d/%d", qPrintable(full_path),
//		max_img_w, max_img_h, img.width(), img.height());
	
	auto *thumbnail = new Thumbnail();
	thumbnail->abi_version = CurrentThumbnailAbi;
	thumbnail->img = img;
	thumbnail->file_id = file_id;
	thumbnail->time_generated = time(NULL);
	thumbnail->w = (int)used_w;
	thumbnail->h = (int)used_h;
	thumbnail->original_image_w = sz.width();
	thumbnail->original_image_h = sz.height();
	thumbnail->tab_id = tab_id;
	thumbnail->dir_id = dir_id;
	
	return thumbnail;
}

void* GlobalThumbLoadMonitor(void *args)
{
	pthread_detach(pthread_self());
	GlobalThumbLoaderData *global_data = (GlobalThumbLoaderData*)args;
	auto global_guard = global_data->guard();
	
	while (!global_data->threads.isEmpty())
	{
		auto &work_queue = global_data->work_queue;
		for (ThumbLoaderData *th_data: global_data->threads)
		{
			if (work_queue.isEmpty())
				break;
			auto guard = th_data->guard();
			if (th_data->new_work != nullptr)
				continue;
			
			th_data->new_work = work_queue.takeLast();
			th_data->SignalNewWorkAvailable();
		}
		
		int status = global_data->CondWaitForWorkQueueChange();
		if (status != 0)
		{
			mtl_status(status);
			break;
		}
	}
	
	return nullptr;
}

static void FreeThumbnailMemory(void *data)
{
	uchar *m = (uchar*)data;
	delete[] m;
}

namespace thumbnail {
QImage ImageFromByteArray(ByteArray &ba, i32 &img_w, i32 &img_h)
{
	if (ba.size() <= ThumbnailHeaderSize)
	{
		img_w = img_h = -1;
		return QImage();
	}
	
	const i16 abi_version = ba.next_i16();
	Q_UNUSED(abi_version);
	
	if (abi_version != CurrentThumbnailAbi)
	{
		mtl_warn("Wrong abi: %d", abi_version);
		return QImage();
	}
	
	const int w = ba.next_i32();
	const int h = ba.next_i32();
	img_w = ba.next_i32();
	img_h = ba.next_i32();
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
