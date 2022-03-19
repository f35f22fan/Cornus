#include "thumbnail.hh"

#include "ByteArray.hpp"

#include "io/File.hpp"
#include "io/io.hh"

#include <QImageReader>

void CornusFreeQImageMemory(void *data)
{
//	mtl_info("Memory freed");
	uchar *m = (uchar*)data;
	delete[] m;
	//free(m);
}

namespace cornus {

namespace thumbnail {
bool GetOriginalImageSize(ByteArray &ba, i4 &w, i4 &h)
{
	const auto at = ba.at();
	if (ba.size() <= thumbnail::HeaderSize ||
		(ba.next_i2() != thumbnail::AbiVersion))
	{
		return false;
	}
	
	ba.to(at + 8);
	w = ba.next_i4();
	h = ba.next_i4();
	ba.to(at);
	
	return true;
}

QImage ImageFromByteArray(ByteArray &ba, i4 &img_w, i4 &img_h,
	AbiType &abi_version, ZSTD_DCtx *decompress_ctx)
{
	if (ba.size() <= thumbnail::HeaderSize ||
		((abi_version = ba.next_i2()) != thumbnail::AbiVersion))
	{
		return QImage();
	}
	
	const int thmb_w = ba.next_i2();
	const int thmb_h = ba.next_i2();
	const int bpl = ba.next_u2();
	img_w = ba.next_i4();
	img_h = ba.next_i4();
	const auto format = static_cast<QImage::Format>(ba.next_i4());
	ba.to(0); // leave ByteArray in same state it was passed in
	char *src_buf = (ba.data() + thumbnail::HeaderSize);
	const i8 src_size = ba.size() - thumbnail::HeaderSize;
	
	const i8 dst_size = ZSTD_getFrameContentSize(src_buf, src_size);
	uchar *dst_buf = new uchar[dst_size];
	const i8 decompressed_size = ZSTD_decompressDCtx(decompress_ctx,
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

Thumbnail* Load(const QString &full_path, const u8 &file_id,
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
	}
	
	//mtl_info("%s is null: %s", qPrintable(full_path), img.isNull() ? "true" : "false");
	if (img.isNull()) {
		return nullptr;
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
//		mtl_trace();
		return img;
	}
	
	if (WebPGetFeatures((const u1*)ba.data(), ba.size(), &config.input) != VP8_STATUS_OK)
	{
//		mtl_trace();
		return img;
	}
	
	config.options.no_fancy_upsampling = 1;
	const QSize img_sz(config.input.width, config.input.height);
	orig_img_sz = img_sz;
	scaled_sz = GetScaledSize(img_sz, max_img_w, max_img_h);
	config.options.use_scaling = 1;
	config.options.scaled_width = scaled_sz.width();
	config.options.scaled_height = scaled_sz.height();
	
	const i8 scanline_stride = scaled_sz.width() * 4;
//	mtl_info("Requested stride: %ld", scanline_stride);
//	mtl_info("max_img_w: %d, h: %d, scaled w: %d, h: %d",
//		max_img_w, max_img_h, scaled_sz.width(), scaled_sz.height());
	const i8 external_buf_size = scanline_stride * scaled_sz.height();
	uchar *external_buf = new uchar[external_buf_size];
	{
		// Specify the desired output colorspace:
		config.output.colorspace = MODE_RGBA;
		// Have config.output point to an external buffer:
		config.output.u.RGBA.rgba = external_buf;
		config.output.u.RGBA.stride = scanline_stride;
		config.output.u.RGBA.size = external_buf_size;
		config.output.is_external_memory = 1;
	}
	
	if (WebPDecode((const u1*)ba.data(), ba.size(), &config) != VP8_STATUS_OK)
	{
		mtl_trace();
		return img;
	}
	
	WebPFreeDecBuffer(&config.output);
	
	img = QImage(external_buf, config.output.width, config.output.height,
		scanline_stride, QImage::Format_RGBA8888,
		CornusFreeQImageMemory, external_buf);
	
	return img;
}

}} // cornus::thumbnail::
