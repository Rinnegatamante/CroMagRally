#include "Pomme.h"
#include "PommeGraphics.h"
#include "Utilities/bigendianstreams.h"

#include <list>
#include <fstream>
#include <iostream>
#include <vector>

using namespace Pomme;
using namespace Pomme::Graphics;

#define LOG POMME_GENLOG(POMME_DEBUG_PICT, "PICT")
#define LOG_NOPREFIX POMME_GENLOG_NOPREFIX(POMME_DEBUG_PICT)

class PICTException : public std::runtime_error
{
public:
	PICTException(const char* m) : std::runtime_error(m)
	{}
};

//-----------------------------------------------------------------------------
// Rect helpers

Rect ReadRect(BigEndianIStream& f)
{
	Rect r;
	r.top    = f.Read<SInt16>();
	r.left   = f.Read<SInt16>();
	r.bottom = f.Read<SInt16>();
	r.right  = f.Read<SInt16>();
	return r;
}

bool operator==(const Rect& r1, const Rect& r2)
{
	return
		r1.top == r2.top &&
		r1.left == r2.left &&
		r1.bottom == r2.bottom &&
		r1.right == r2.right;
}

bool operator!=(const Rect& r1, const Rect& r2)
{
	return
		r1.top != r2.top ||
		r1.left != r2.left ||
		r1.bottom != r2.bottom ||
		r1.right != r2.right;
}

std::ostream& operator<<(std::ostream& s, const Rect& r)
{
	return s << "T" << r.top << "L" << r.left << "B" << r.bottom << "R" << r.right;
}

//-----------------------------------------------------------------------------
// Dump Targa

void Pomme::Graphics::DumpTGA(const char* path, short width, short height, const char* argbData)
{
	std::ofstream tga(path, std::ios::binary);
	uint16_t tgaHdr[] = {0, 2, 0, 0, 0, 0, (uint16_t) width, (uint16_t) height, 0x2820};
	tga.write((const char*) tgaHdr, sizeof(tgaHdr));
	for (int i = 0; i < 4 * width * height; i += 4)
	{
		tga.put(argbData[i + 3]); //b
		tga.put(argbData[i + 2]); //g
		tga.put(argbData[i + 1]); //r
		tga.put(argbData[i + 0]); //a
	}
	tga.close();
}

//-----------------------------------------------------------------------------
// PackBits

template<typename T>
static std::vector<T> UnpackBits(BigEndianIStream& f, UInt16 rowbytes, int packedLength)
{
	//LOG << "UnpackBits rowbytes=" << rowbytes << " packedlength=" << packedLength << "\n";

	std::vector<T> unpacked;

	if (rowbytes < 8)
	{
		// Bits aren't compressed
		LOG << "Bits aren't compressed\n";
		for (int j = 0; j < packedLength; j += sizeof(T))
		{
			unpacked.push_back(f.Read<T>());
		}
		return unpacked;
	}

	for (int j = 0; j < packedLength;)
	{
		Byte FlagCounter = f.Read<Byte>();

		if (FlagCounter == 0x80)
		{
			// special case: repeat value of 0. Apple says ignore.
			j++;
		}
		else if (FlagCounter & 0x80)
		{
			// Packed data.
			int len = ((FlagCounter ^ 0xFF) & 0xFF) + 2;
			auto item = f.Read<T>();
			for (int k = 0; k < len; k++)
			{
				unpacked.push_back(item);
			}
			j += 1 + sizeof(T);
		}
		else
		{
			// Unpacked data
			int len = (FlagCounter & 0xFF) + 1;
			for (int k = 0; k < len; k++)
			{
				unpacked.push_back(f.Read<T>());
			}
			j += 1 + len * sizeof(T);
		}
	}

	return unpacked;
}

//-----------------------------------------------------------------------------
// Unpack PICT pixmap formats

template<typename T>
static std::vector<T> UnpackAllRows(BigEndianIStream& f, int w, int h, UInt16 rowbytes, std::size_t expectedItemCount)
{
	(void) w;

	LOG << "UnpackBits<" << typeid(T).name() << ">";

	std::vector<T> data;
	data.reserve(expectedItemCount);
	for (int y = 0; y < h; y++)
	{
		int packedRowBytes = rowbytes > 250 ? f.Read<UInt16>() : f.Read<UInt8>();
		std::vector<T> rowPixels = UnpackBits<T>(f, rowbytes, packedRowBytes);
		data.insert(data.end(), rowPixels.begin(), rowPixels.end());
	}

	if (expectedItemCount != data.size())
	{
		throw PICTException("UnpackAllRows: unexpected item count");
	}

	LOG_NOPREFIX << "\n";
	return data;
}

// Unpack pixel type 0 (8-bit indexed)
static ARGBPixmap Unpack0(BigEndianIStream& f, int w, int h, const std::vector<Color>& palette)
{
	auto unpacked = UnpackAllRows<UInt8>(f, w, h, w, w * h);
	ARGBPixmap dst(w, h);
	dst.data.clear();
	LOG << "indexed to RGBA";
	for (uint8_t px : unpacked)
	{
		if (px >= palette.size())
		{
			throw PICTException("Unpack0: illegal color index in pixmap");
		}
		Color c = palette[px];
		dst.data.push_back(c.a);
		dst.data.push_back(c.r);
		dst.data.push_back(c.g);
		dst.data.push_back(c.b);
	}
	LOG_NOPREFIX << "\n";
	return dst;
}

// Unpack pixel type 4 (16 bits, chunky)
static ARGBPixmap Unpack3(BigEndianIStream& f, int w, int h, UInt16 rowbytes)
{
	auto unpacked = UnpackAllRows<UInt16>(f, w, h, rowbytes, w * h);
	ARGBPixmap dst(w, h);
	dst.data.clear();
	dst.data.reserve(unpacked.size() * 4);
	LOG << "Chunky16 to RGBA";
	for (UInt16 px : unpacked)
	{
		UInt8 r = ((px >> 10) & 0b11111) * 255 / 31;
		UInt8 g = ((px >>  5) & 0b11111) * 255 / 31;
		UInt8 b = ((px >>  0) & 0b11111) * 255 / 31;

		dst.data.push_back(0xFF); // a
		dst.data.push_back(r);
		dst.data.push_back(g);
		dst.data.push_back(b);
	}
	LOG_NOPREFIX << "\n";
	return dst;
}

// Unpack pixel type 4 (24 or 32 bits, planar)
static ARGBPixmap Unpack4(BigEndianIStream& f, int w, int h, UInt16 rowbytes, int numPlanes)
{
	auto unpacked = UnpackAllRows<Byte>(f, w, h, rowbytes, numPlanes * w * h);
	ARGBPixmap dst(w, h);
	dst.data.clear();
	LOG << "Planar" << numPlanes*8 << " to RGBA";
	for (int y = 0; y < h; y++)
	{
		if (numPlanes == 3)
		{
			for (int x = 0; x < w; x++)
			{
				dst.data.push_back(0xFF);
				dst.data.push_back(unpacked[y*w*3 + x + w*0]); // red
				dst.data.push_back(unpacked[y*w*3 + x + w*1]); // grn
				dst.data.push_back(unpacked[y*w*3 + x + w*2]); // blu
			}
		}
		else
		{
			for (int x = 0; x < w; x++)
			{
				dst.data.push_back(unpacked[y*w*3 + x + w*0]); // alpha
				dst.data.push_back(unpacked[y*w*3 + x + w*1]); // red
				dst.data.push_back(unpacked[y*w*3 + x + w*2]); // grn
				dst.data.push_back(unpacked[y*w*3 + x + w*3]); // blu
			}
		}
	}
	LOG_NOPREFIX << "\n";
	return dst;
}

//-----------------------------------------------------------------------------
// PICT header

static ARGBPixmap ReadPICTBits(BigEndianIStream& f, int opcode, const Rect& canvasRect)
{
	bool directBitsOpcode = opcode == 0x009A || opcode == 0x009B;

	if (directBitsOpcode) f.Skip(4); //skip junk

	UInt16 rowbytes = f.Read<UInt16>();

	// Get rid of MSB in rowbytes so we pass a real length to UnpackXXX functions
	bool rowbytesMSBWasHigh = 0 != (rowbytes & 0x8000);
	rowbytes &= 0x7FFF;

	Rect frameRect = ReadRect(f);
	LOG << "frameRect " << frameRect << "\n";
	if (frameRect != canvasRect)
		throw PICTException("frame dims != canvas dims 1");

	int packType = -1;
	int pixelSize = -1;
	int componentCount = -1;

	SInt16 pixmapVersion	= f.Read<SInt16>();
	packType			    = f.Read<SInt16>();
	SInt32 packSize			= f.Read<SInt32>();
	Fixed hResolution		= f.Read<Fixed >();
	Fixed vResolution		= f.Read<Fixed >();
	SInt16 pixelType		= f.Read<SInt16>();
	pixelSize				= f.Read<UInt16>();
	componentCount			= f.Read<UInt16>();
	SInt16 componentSize	= f.Read<SInt16>();
	SInt32 planeBytes		= f.Read<SInt32>();
	SInt32 table			= f.Read<SInt32>();
	if (directBitsOpcode || rowbytesMSBWasHigh)
	{
		f.Skip(4);

		LOG << "----PICT PIXMAP----"
			<< "\n\tpixmap version  " << pixmapVersion
			<< "\n\tpack type       " << packType
			<< "\n\tpack size       " << packSize
			<< "\n\tresolution      " << (hResolution>>16) << "x" << (vResolution>>16)
			<< "\n\tpixel type      " << pixelType
			<< "\n\tbpp             " << pixelSize
			<< "\n\tcomponent count " << componentCount
			<< "\n\tcomponent size  " << componentSize
			<< "\n\tplane bytes     " << planeBytes
			<< "\n\ttable           " << table
			<< "\n\trowbytes        " << rowbytes << " (MSB " << (rowbytesMSBWasHigh? "hi": "lo") << ")"
			<< "\n";

		if (pixelSize > 32) throw PICTException("pixmap invalid bpp");
		if (componentCount > 4) throw PICTException("pixmap invalid component count");
		if (componentSize <= 0) throw PICTException("pixmap invalid component size");
	}
	else
	{
		LOG << "PICT: neither directBitsOpcode nor rowbytesMSBWasHigh\n";
	}

	auto palette = std::vector<Color>();
	if (!directBitsOpcode)
	{
		f.Skip(4);
		UInt16 flags = f.Read<UInt16>();
		int nColors = 1 + f.Read<UInt16>();

		LOG << "Colormap: " << nColors << " colors\n";
		if (nColors <= 0 || nColors > 256) throw PICTException("unsupported palette size");

		palette.resize(nColors);

		for (int i = 0; i < nColors; i++)
		{
			int index;
			if (flags & 0x8000)
			{
				// ignore junk index (usually just set to 0)
				f.Skip(2);
				index = i;
			}
			else
			{
				index = f.Read<UInt16>();
				if (index >= nColors)
					throw PICTException("illegal color index in palette definition");
			}

			UInt8 r = (f.Read<UInt16>() >> 8) & 0xFF;
			UInt8 g = (f.Read<UInt16>() >> 8) & 0xFF;
			UInt8 b = (f.Read<UInt16>() >> 8) & 0xFF;
			palette[index] = Color(r, g, b);
		}
	}

	Rect srcRect = ReadRect(f);
	Rect dstRect = ReadRect(f);
	if (srcRect != dstRect) throw PICTException("unsupported src/dst rects");
	if (srcRect != canvasRect) throw PICTException("unsupported src/dst rects that aren't the same as the canvas rect");
	f.Skip(2);

	if (opcode == 0x0091 || opcode == 0x0099 || opcode == 0x009b)
	{
		throw PICTException("unimplemented opcode");
	}

	int cw = Width(canvasRect);
	int ch = Height(canvasRect);

	switch (packType)
	{
		case 0: // 8-bit indexed color, packed bytewise
			return Unpack0(f, cw, ch, palette);

		case 3: // 16-bit color, stored chunky, packed pixelwise
			return Unpack3(f, cw, ch, rowbytes);

		case 4: // 24- or 32-bit color, stored planar, packed bytewise
			return Unpack4(f, cw, ch, rowbytes, componentCount);

		default:
			throw PICTException("don't know how to unpack this pixel size");
	}
}

ARGBPixmap Pomme::Graphics::ReadPICT(std::istream& theF, bool skip512)
{
	BigEndianIStream f(theF);

	LOG << "-----------------------------\n";
	auto startOff = f.Tell();

	if (skip512)
		f.Skip(512); // junk

	f.Skip(2); // Version 1 picture size. Meaningless for "modern" picts that can easily exceed 65,535 bytes.

	Rect canvasRect = ReadRect(f);
	if (Width(canvasRect) < 0 || Height(canvasRect) < 0)
	{
		LOG << "canvas rect: " << canvasRect << "\n";
		throw PICTException("invalid width/height");
	}

	LOG << std::dec << "Pict canvas: " << canvasRect << "\n";

	if (0x0011 != f.Read<SInt16>())
		throw PICTException("didn't find version opcode in PICT header");
	if (0x02 != f.Read<Byte>()) throw PICTException("unrecognized PICT version");
	if (0xFF != f.Read<Byte>()) throw PICTException("bad PICT header");

	ARGBPixmap pm(0, 0);
	bool readPixmap = false;

	while (true)
	{
		int opcode;

		// Align position to short
		f.Skip((f.Tell() - startOff) % 2);

		opcode = f.Read<SInt16>();

		// Skip reserved opcodes
		if (opcode >= 0x0100 && opcode <= 0x7FFF)
		{
			f.Skip((opcode >> 7) & 0xFF);
			continue;
		}

		switch (opcode)
		{
		case 0x0000: // nop
		case 0x001E: // DefHilite
		case 0x0048: // frameSameRRect
			LOG << __func__ << ": skipping opcode " << opcode << "\n";
			break;

		case 0x001F: // OpColor
			LOG << __func__ << ": skipping opcode " << opcode << "\n";
			f.Skip(6);
			break;

		case 0x0001: // clip
		{
			auto length = f.Read<UInt16>();
			if (length != 0x0A) f.Skip(length - 2);
			Rect frameRect = ReadRect(f);
			//LOG << "CLIP:" << frameRect << "\n";
			if (frameRect.left < 0 || frameRect.top < 0) throw PICTException("illegal frame rect");
			if (frameRect != canvasRect)
			{
				std::cerr << "Warning: clip rect " << frameRect << " isn't the same as the canvas rect " << canvasRect << ", using clip rect\n";
				canvasRect = frameRect;
			}
			break;
		}

		case 0x0098: // PackBitsRect
		case 0x009A: // DirectBitsRect
			if (readPixmap)
				throw PICTException("already read one pixmap!");
			pm = ReadPICTBits(f, opcode, canvasRect);
			readPixmap = true;
			break;

		case 0x00A1: // Long comment
			f.Skip(2);
			f.Skip(f.Read<UInt16>());
			break;

		case 0x00FF: // done
		case 0xFFFF:
			return pm;

		default:
			std::cerr << "unsupported opcode " << opcode << " at offset " << f.Tell() << "\n";
			throw PICTException("unsupported PICT opcode");
		}
	}

	return pm;
}
