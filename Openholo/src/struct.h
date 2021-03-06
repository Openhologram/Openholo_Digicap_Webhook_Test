#ifndef __struct_h
#define __struct_h

namespace oph
{
	// for PointCloud
	typedef struct OPH_DLL KernelConst {
		int n_points;	///number of point cloud

		double scaleX;		/// Scaling factor of x coordinate of point cloud
		double scaleY;		/// Scaling factor of y coordinate of point cloud
		double scaleZ;		/// Scaling factor of z coordinate of point cloud

		double offsetDepth;	/// Offset value of point cloud in z direction

		int Nx;		/// Number of pixel of SLM in x direction
		int Ny;		/// Number of pixel of SLM in y direction

		double sin_thetaX; ///sin(tiltAngleX)
		double sin_thetaY; ///sin(tiltAngleY)
		double k;		  ///Wave Number = (2 * PI) / lambda;

		double pixel_x; /// Pixel pitch of SLM in x direction
		double pixel_y; /// Pixel pitch of SLM in y direction
		double halfLength_x; /// (pixel_x * nx) / 2
		double halfLength_y; /// (pixel_y * ny) / 2
	} GpuConst;
}

#pragma pack(push,1)
typedef struct {
	uint8_t signature[2];
	uint32_t filesize;
	uint32_t reserved;
	uint32_t fileoffset_to_pixelarray;
} fileheader;
typedef struct {
	uint32_t dibheadersize;
	uint32_t width;
	uint32_t height;
	uint16_t planes;
	uint16_t bitsperpixel;
	uint32_t compression;
	uint32_t imagesize;
	uint32_t ypixelpermeter;
	uint32_t xpixelpermeter;
	uint32_t numcolorspallette;
	uint32_t mostimpcolor;
} bitmapinfoheader;
typedef struct {
	uint8_t rgbBlue;
	uint8_t rgbGreen;
	uint8_t rgbRed;
	uint8_t rgbReserved;
} rgbquad;
typedef struct {
	fileheader fileheader;
	bitmapinfoheader bitmapinfoheader;
	rgbquad rgbquad[256];
} bitmap;
#pragma pack(pop)

typedef struct {
	uint8_t signature[3];
	uint64_t filesize;
	uint32_t offsetbits;
}ohfheader;

typedef struct {
	uint32_t headersize;
	uint32_t pixelnum_x;
	uint32_t pixelnum_y;
	double pixelpitch_x;
	double pixelpitch_y;
	uint8_t pitchunit[2];
	uint8_t colortype[3];
	double wavelength;
	uint8_t wavelengthunit[2];
	uint8_t complexfieldtype[6];
	uint8_t fieldtype[2];
	uint8_t imageformat[10];
}ohffieldheader;

typedef struct {
	ohfheader file_header;
	ohffieldheader field_header;
}ohfdata;

#endif // !__struct_h