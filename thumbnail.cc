#include "thumbnail.hh"

#include "ByteArray.hpp"

#include "io/io.hh"

#include <QImageReader>

void CornusFreeQImageMemory(void *data)
{
//	mtl_info("Memory freed");
	uchar *m = (uchar*)data;
	free(m);
	//delete[] m;
}

namespace cornus::thumbnail {

QImage ImageFromByteArray(ByteArray &ba, i32 &img_w, i32 &img_h,
	ZSTD_DCtx *decompress_context)
{
	if (ba.size() <= ThumbnailHeaderSize)
	{
		img_w = img_h = -1;
		return QImage();
	}
	
	const i16 abi_version = ba.next_i16();
	Q_UNUSED(abi_version);
	
	if (abi_version != thumbnail::AbiVersion)
	{
		mtl_warn("Wrong abi: %d", (int)abi_version);
		return QImage();
	}
	
	const int thmb_w = ba.next_i16();
	const int thmb_h = ba.next_i16();
	const int bpl = ba.next_u16();
	img_w = ba.next_i32();
	img_h = ba.next_i32();
	const auto format = static_cast<QImage::Format>(ba.next_i32());
	ba.to(0);
	char *src_buf = (ba.data() + ThumbnailHeaderSize);
	const i64 src_size = ba.size() - ThumbnailHeaderSize;
	
	const i64 dst_size = ZSTD_getFrameContentSize(src_buf, src_size);
	uchar *dst_buf = (uchar*) malloc(dst_size);// new uchar[dst_size];
	const i64 decompressed_size = ZSTD_decompressDCtx(decompress_context,
		dst_buf, dst_size, src_buf, src_size);
	Q_UNUSED(decompressed_size);
	
	return QImage(dst_buf, thmb_w, thmb_h, bpl, format, CornusFreeQImageMemory, dst_buf);
}

QSize GetScaledSize(const QSize &input, const int max_img_w,
	const int max_img_h)
{
	const double pw = input.width();
	const double ph = input.height();
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
	
	return QSize((int)used_w, (int)used_h);
}

Thumbnail* Load(const QString &full_path, const u64 &file_id,
	const QByteArray &ext, const int max_img_w, const int max_img_h,
	const TabId tab_id, const DirId dir_id)
{
	QSize scaled, orig_img_sz;
	const bool is_webp = (ext == QByteArray("webp"));
	QImage img;
	if (is_webp)
	{ // WebP is disabled in Qt5 on Ubuntu 21.10, so load webp files manually.
		img = LoadWebpImage(full_path, max_img_w, max_img_h, scaled, orig_img_sz);
	} else {
		QImageReader reader = QImageReader(full_path, ext);
		orig_img_sz = reader.size();
		if (orig_img_sz.isEmpty())
			return nullptr;
		
		scaled = GetScaledSize(orig_img_sz, max_img_w, max_img_h);
		reader.setScaledSize(scaled);
		
		img = reader.read();
		if (img.isNull())
		{
			return nullptr;
		}
	}
	
	auto *thumbnail = new Thumbnail();
	thumbnail->abi_version = thumbnail::AbiVersion;
	thumbnail->img = img;
	thumbnail->file_id = file_id;
	thumbnail->time_generated = time(NULL);
	thumbnail->w = scaled.width();
	thumbnail->h = scaled.height();
	thumbnail->original_image_w = orig_img_sz.width();
	thumbnail->original_image_h = orig_img_sz.height();
	thumbnail->tab_id = tab_id;
	thumbnail->dir_id = dir_id;
	
	return thumbnail;
}

void* LoadMonitor(void *args)
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

QImage LoadWebpImage(const QString &full_path, const int max_img_w,
	const int max_img_h, QSize &scaled_sz, QSize &orig_img_sz)
{
	QImage img;
	io::ReadParams rp = {};
	rp.can_rely = CanRelyOnStatxSize::Yes;
	rp.print_errors = PrintErrors::No;
	
	ByteArray ba;
	if (!io::ReadFile(full_path, ba, rp))
		return img;
	
	WebPDecoderConfig config;
	if (!WebPInitDecoderConfig(&config))
	{
		mtl_trace();
		return img;
	}
	
	if (WebPGetFeatures((const u8*)ba.data(), ba.size(), &config.input) != VP8_STATUS_OK)
	{
		mtl_trace();
		return img;
	}
	
	config.options.no_fancy_upsampling = 1;
	const QSize img_sz(config.input.width, config.input.height);
	orig_img_sz = img_sz;
	scaled_sz = GetScaledSize(img_sz, max_img_w, max_img_h);
	config.options.use_scaling = 1;
	config.options.scaled_width = scaled_sz.width();
	config.options.scaled_height = scaled_sz.height();
	
	const i64 scanline_stride = scaled_sz.width() * 4;
	mtl_info("Requested stride: %ld", scanline_stride);
	mtl_info("max_img_w: %d, h: %d, scaled w: %d, h: %d",
		max_img_w, max_img_h, scaled_sz.width(), scaled_sz.height());
	const i64 memory_buf_size = scanline_stride * scaled_sz.height();
	uchar *memory_buf = (uchar*)malloc(memory_buf_size);
	{
		// Specify the desired output colorspace:
		config.output.colorspace = MODE_RGBA;
		// Have config.output point to an external buffer:
		config.output.u.RGBA.rgba = memory_buf;
		config.output.u.RGBA.stride = scanline_stride;
		config.output.u.RGBA.size = memory_buf_size;
		config.output.is_external_memory = 1;
	}
	
	if (WebPDecode((const u8*)ba.data(), ba.size(), &config) != VP8_STATUS_OK)
	{
		mtl_trace();
		return img;
	}
	
	auto &buf = config.output.u.RGBA;
	const QSize thmb_sz(config.output.width, config.output.height);
	mtl_info("Scanline stride: %d, w: %d, h:%d", buf.stride,
		thmb_sz.width(), thmb_sz.height());
	
	img = QImage(memory_buf, thmb_sz.width(), thmb_sz.height(),
		(int)buf.stride, QImage::Format_RGBA8888,
		CornusFreeQImageMemory, memory_buf);
	
	WebPFreeDecBuffer(&config.output);
	
	return img;
}

} // cornus::thumbnail::
