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

Thumbnail* Thumbnail::Clone() {
	Thumbnail *t = new Thumbnail();
	t->debug_path = debug_path;
	t->img = img;
	t->file_id = file_id;
	t->time_generated = time_generated;
	t->tab_id = tab_id;
	t->dir_id = dir_id;
	t->w = w;
	t->h = h;
	t->original_image_w = original_image_w;
	t->original_image_h = original_image_h;
	t->abi_version = abi_version;
	t->origin = origin;
	
	return t;
}

namespace thumbnail {
bool GetOriginalImageSize(ByteArray &ba, i32 &w, i32 &h)
{
	const auto at = ba.at();
	if (ba.size() <= thumbnail::HeaderSize ||
		(ba.next_i16() != thumbnail::AbiVersion))
	{
		return false;
	}
	
	ba.to(at + 8);
	w = ba.next_i32();
	h = ba.next_i32();
	ba.to(at);
	
	return true;
}

QImage ImageFromByteArray(ByteArray &ba, i32 &img_w, i32 &img_h,
	AbiType &abi_version, ZSTD_DCtx *decompress_ctx)
{
	if (ba.size() <= thumbnail::HeaderSize ||
		((abi_version = ba.next_i16()) != thumbnail::AbiVersion))
	{
		return QImage();
	}
	
	cint thmb_w = ba.next_i16();
	cint thmb_h = ba.next_i16();
	cint bpl = ba.next_u16();
	img_w = ba.next_i32();
	img_h = ba.next_i32();
	const auto format = static_cast<QImage::Format>(ba.next_i32());
	ba.to(0); // leave ByteArray in same state it was passed in
	char *src_buf = ba.data() + thumbnail::HeaderSize;
	ci64 src_size = ba.size() - thumbnail::HeaderSize;
	
	ci64 dst_size = ZSTD_getFrameContentSize(src_buf, src_size);
	uchar *dst_buf = new uchar[dst_size];
	ci64 decompressed_size = ZSTD_decompressDCtx(decompress_ctx,
		dst_buf, dst_size, src_buf, src_size);
	Q_UNUSED(decompressed_size);
	
	return QImage(dst_buf, thmb_w, thmb_h, bpl, format, CornusFreeQImageMemory, dst_buf);
}

QSize GetScaledSize(const QSize &input, cint max_img_w,
	cint max_img_h)
{
	const double pw = input.width();
	const double ph = input.height();
	double used_w, used_h;
	if (pw > max_img_w || ph > max_img_h)
	{
		cf64 ratio = std::max(pw / max_img_w, ph / max_img_h);
		used_w = pw / ratio;
		used_h = ph / ratio;
	} else {
		used_w = pw;
		used_h = ph;
	}
	
	return QSize((int)used_w, (int)used_h);
}

Thumbnail* Load(const QString &full_path, const u64 &file_id,
	const QByteArray &ext, cint max_img_w, cint max_img_h,
	const TabId tab_id, const DirId dir_id)
{
	QSize scaled, orig_img_sz;
	QImage img;
	// if (ext == QByteArray("webp")) {
	// 	img = LoadWebpImage(full_path, max_img_w, max_img_h, scaled, orig_img_sz);
	// } else {
	QImageReader reader = QImageReader(full_path, ext);
	orig_img_sz = reader.size();
	if (orig_img_sz.isEmpty()) {
		mtl_warn("%s", qPrintable(full_path));
		return nullptr;
	}
	
	scaled = GetScaledSize(orig_img_sz, max_img_w, max_img_h);
	reader.setScaledSize(scaled);
	img = reader.read();
	if (img.isNull()) {
		mtl_warn("%s (\"%s\") is Null(): ", qPrintable(full_path), ext.data());
		return nullptr;
	}
	
	auto *thumbnail = new Thumbnail();
	thumbnail->debug_path = full_path;
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
		
		cint status = global_data->CondWaitForWorkQueueChange();
		if (status != 0)
		{
			mtl_status(status);
			break;
		}
	}
	
	return nullptr;
}

/*
QImage LoadWebpImage(const QString &full_path, cint max_img_w,
	cint max_img_h, QSize &scaled_sz, QSize &orig_img_sz)
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
		return img;
	}
	
	if (WebPGetFeatures((const u8*)ba.data(), ba.size(), &config.input) != VP8_STATUS_OK)
	{
		return img;
	}
	
	config.options.no_fancy_upsampling = 1;
	const QSize img_sz(config.input.width, config.input.height);
	orig_img_sz = img_sz;
	scaled_sz = GetScaledSize(img_sz, max_img_w, max_img_h);
	config.options.use_scaling = 1;
	config.options.scaled_width = scaled_sz.width();
	config.options.scaled_height = scaled_sz.height();
	
	ci64 scanline_stride = scaled_sz.width() * 4;
	ci64 external_buf_size = scanline_stride * scaled_sz.height();
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
	
	if (WebPDecode((const u8*)ba.data(), ba.size(), &config) != VP8_STATUS_OK)
	{
		mtl_trace();
		return img;
	}
	
	WebPFreeDecBuffer(&config.output);
	
	img = QImage(external_buf, config.output.width, config.output.height,
		scanline_stride, QImage::Format_RGBA8888,
		CornusFreeQImageMemory, external_buf);
	
	return img;
} */

}} // cornus::thumbnail::
