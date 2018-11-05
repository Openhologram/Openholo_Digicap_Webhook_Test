#define OPH_DM_EXPORT 

#include	"ophDepthMap.h"
#include	<windows.h>
#include	<random>
#include	<iomanip>
#include	<io.h>
#include	<direct.h>
#include    "sys.h"

/** 
* @brief Constructor
* @details Initialize variables.
*/
ophDepthMap::ophDepthMap()
{
	isCPU_ = true;

	// GPU Variables
	img_src_gpu_ = 0;
	dimg_src_gpu_ = 0;
	depth_index_gpu_ = 0;

	// CPU Variables
	img_src_ = 0;
	dmap_src_ = 0;
	alpha_map_ = 0;
	depth_index_ = 0;
	dmap_ = 0;
	dstep_ = 0;
	dlevel_.clear();
	U_complex_ = nullptr;
}

/**
* @brief Destructor 
*/
ophDepthMap::~ophDepthMap()
{
}

/**
* @brief Set the value of a variable isCPU_(true or false)
* @details <pre>
    if isCPU_ == true
	   CPU implementation
	else
	   GPU implementation </pre>
* @param isCPU : the value for specifying whether the hologram generation method is implemented on the CPU or GPU
*/
void ophDepthMap::setMode(bool isCPU) 
{ 
	isCPU_ = isCPU; 
}

/**
* @brief Read parameters from a config file(config_openholo.txt).
* @return true if config infomation are sucessfully read, flase otherwise.
*/
bool ophDepthMap::readConfig(const char* fname)
{
	bool b_ok = ophGen::readConfig(fname, dm_config_, dm_params_, dm_simuls_);

	return b_ok;
}

/**
* @brief Initialize variables for CPU and GPU implementation.
* @see init_CPU, init_GPU
*/
void ophDepthMap::initialize()
{
	dstep_ = 0;
	dlevel_.clear();

	if (p_hologram)		free(p_hologram);
	p_hologram = (real*)malloc(sizeof(real) * context_.pixel_number[0] * context_.pixel_number[1]);
	
	if (isCPU_)
		init_CPU();
	else
		init_GPU();	
}

/**
* @brief Generate a hologram, main funtion.
* @details For each frame, 
*    1. Read image depth data.
*    2. Compute the physical distance of depth map.
*    3. Transform target object to reflect the system configuration of holographic display.
*    4. Generate a hologram.
*    5. Encode the generated hologram.
*    6. Write the hologram to a image.
* .
* @see readImageDepth, getDepthValues, transformViewingWindow, calc_Holo_by_Depth, encodingSymmetrization, writeResultimage
*/
void ophDepthMap::generateHologram()
{
	int num_of_frame;
	if (dm_params_.FLAG_STATIC_IMAGE == 0)
		num_of_frame = dm_params_.NUMBER_OF_FRAME;
	else
		num_of_frame = 1;

	for (int ftr = 0; ftr <= num_of_frame - 1; ftr++)
	{
		LOG("Calculating hologram of frame %d.\n", ftr);

		if (!readImageDepth(ftr)) {
			LOG("Error: Reading image of frame %d.\n", ftr);
			continue;
		}

		getDepthValues();

		if (dm_params_.Transform_Method_ == 0)
			transformViewingWindow();

		calc_Holo_by_Depth(ftr);
		
		if (dm_params_.Encoding_Method_ == 0)
			encodingSymmetrization(ivec2(0,1));

		writeResultimage(ftr);
	}
}

/**
* @brief Read image and depth map.
* @details Read input files and load image & depth map data.
*  If the input image size is different with the dislay resolution, resize the image size.
* @param ftr : the frame number of the image.
* @return true if image data are sucessfully read, flase otherwise.
* @see prepare_inputdata_CPU, prepare_inputdata_GPU
*/
int ophDepthMap::readImageDepth(int ftr)
{	
	std::string src_folder;
	if (dm_params_.FLAG_STATIC_IMAGE == 0)
	{
		if (dm_params_.NUMBER_OF_DIGIT_OF_FRAME_NUMBERING > 0) {
			//src_folder = std::string().append(SOURCE_FOLDER).append("/") + QString("%1").arg((uint)(ftr + START_OF_FRAME_NUMBERING), (int)NUMBER_OF_DIGIT_OF_FRAME_NUMBERING, 10, (QChar)'0');
			std::stringstream ss;
			ss << std::setw(dm_params_.NUMBER_OF_DIGIT_OF_FRAME_NUMBERING) << std::setfill('0') << ftr + dm_params_.START_OF_FRAME_NUMBERING;
			src_folder = ss.str();
		} else
			src_folder = std::string().append(dm_params_.SOURCE_FOLDER).append("/").append(std::to_string(ftr + dm_params_.START_OF_FRAME_NUMBERING));

	}else 
		src_folder = std::string().append(dm_params_.SOURCE_FOLDER);

	
	std::string sdir = std::string("./").append(src_folder).append("/").append(dm_params_.IMAGE_PREFIX).append("*.bmp");

	_finddatai64_t fd;
	intptr_t handle;
	handle = _findfirst64(sdir.c_str(), &fd); 
	if (handle == -1)
	{
		LOG("Error: Source image does not exist: %s.\n", sdir.c_str());
		return false;
	}

	std::string imgfullname = std::string("./").append(src_folder).append("/").append(fd.name);

	int w, h, bytesperpixel;
	int ret = getImgSize(w, h, bytesperpixel, imgfullname.c_str());

	unsigned char* imgload = (unsigned char*)malloc(sizeof(unsigned char)*w*h*bytesperpixel);
	ret = loadAsImg(imgfullname.c_str(), (void*)imgload);
	if (!ret) {
		LOG("Failed::Image Load: %s\n", imgfullname.c_str());
		return false;
	}
	LOG("Succeed::Image Load: %s\n", imgfullname.c_str());

	unsigned char* img = (unsigned char*)malloc(sizeof(unsigned char)*w*h);
	convertToFormatGray8(img, imgload, w, h, bytesperpixel);

	//ret = creatBitmapFile(img, w, h, 8, "load_img.bmp");

	//QImage qtimg(img, w, h, QImage::Format::Format_Grayscale8);
	//qtimg.save("load_qimg.bmp");

	free(imgload);


	//=================================================================================
	std::string sddir = std::string("./").append(src_folder).append("/").append(dm_params_.DEPTH_PREFIX).append("*.bmp");
	handle = _findfirst64(sddir.c_str(), &fd);
	if (handle == -1)
	{
		LOG("Error: Source depthmap does not exist: %s.\n", sddir);
		return false;
	}

	std::string dimgfullname = std::string("./").append(src_folder).append("/").append(fd.name);

	int dw, dh, dbytesperpixel;
	ret = getImgSize(dw, dh, dbytesperpixel, dimgfullname.c_str());

	unsigned char* dimgload = (unsigned char*)malloc(sizeof(unsigned char)*dw*dh*dbytesperpixel);
	ret = loadAsImg(dimgfullname.c_str(), (void*)dimgload);
	if (!ret) {
		LOG("Failed::Depth Image Load: %s\n", dimgfullname.c_str());
		return false;
	}
	LOG("Succeed::Depth Image Load: %s\n", dimgfullname.c_str());

	unsigned char* dimg = (unsigned char*)malloc(sizeof(unsigned char)*dw*dh);
	convertToFormatGray8(dimg, dimgload, dw, dh, dbytesperpixel);

	free(dimgload);

	//ret = creatBitmapFile(dimg, dw, dh, 8, "dtest");
	//=======================================================================
	//resize image
	int pnx = context_.pixel_number[0];
	int pny = context_.pixel_number[1];

	unsigned char* newimg = (unsigned char*)malloc(sizeof(char)*pnx*pny);
	memset(newimg, 0, sizeof(char)*pnx*pny);

	if (w != pnx || h != pny)
		imgScaleBilnear(img, newimg, w, h, pnx, pny);
	else
		memcpy(newimg, img, sizeof(char)*pnx*pny);

	//ret = creatBitmapFile(newimg, pnx, pny, 8, "stest");

	unsigned char* newdimg = (unsigned char*)malloc(sizeof(char)*pnx*pny);
	memset(newdimg, 0, sizeof(char)*pnx*pny);

	if (dw != pnx || dh != pny)
		imgScaleBilnear(dimg, newdimg, dw, dh, pnx, pny);
	else
		memcpy(newdimg, dimg, sizeof(char)*pnx*pny);

	if (isCPU_)
		ret = prepare_inputdata_CPU(newimg, newdimg);
	else
		ret = prepare_inputdata_GPU(newimg, newdimg);

	free(img);
	free(dimg);

	//writeIntensity_gray8_bmp("test.bmp", pnx, pny, dmap_src_);
	//writeIntensity_gray8_bmp("test2.bmp", pnx, pny, dmap_);
	//dimg.save("test_dmap.bmp");
	//img.save("test_img.bmp");

	return ret;
	
}

/**
* @brief Calculate the physical distances of depth map layers
* @details Initialize 'dstep_' & 'dlevel_' variables.
*  If FLAG_CHANGE_DEPTH_QUANTIZATION == 1, recalculate  'depth_index_' variable.
* @see change_depth_quan_CPU, change_depth_quan_GPU
*/
void ophDepthMap::getDepthValues()
{
	if (dm_config_.num_of_depth > 1)
	{
		dstep_ = (dm_config_.far_depthmap - dm_config_.near_depthmap) / (dm_config_.num_of_depth - 1);
		real val = dm_config_.near_depthmap;
		while (val <= dm_config_.far_depthmap)
		{
			dlevel_.push_back(val);
			val += dstep_;
		}

	} else {

		dstep_ = (dm_config_.far_depthmap + dm_config_.near_depthmap) / 2;
		dlevel_.push_back(dm_config_.far_depthmap - dm_config_.near_depthmap);

	}
	
	if (dm_params_.FLAG_CHANGE_DEPTH_QUANTIZATION == 1)
	{
		if (isCPU_)
			change_depth_quan_CPU();
		else
			change_depth_quan_GPU();
	}
}

/**
* @brief Transform target object to reflect the system configuration of holographic display.
* @details Calculate 'dlevel_transform_' variable by using 'field_lens' & 'dlevel_'.
*/
void ophDepthMap::transformViewingWindow()
{
	int pnx = context_.pixel_number[0];
	int pny = context_.pixel_number[1];

	real val;
	dlevel_transform_.clear();
	for (int p = 0; p < dlevel_.size(); p++)
	{
		val = -dm_config_.field_lens * dlevel_[p] / (dlevel_[p] - dm_config_.field_lens);
		dlevel_transform_.push_back(val);
	}
}

/**
* @brief Generate a hologram.
* @param frame : the frame number of the image.
* @see calc_Holo_CPU, calc_Holo_GPU
*/
void ophDepthMap::calc_Holo_by_Depth(int frame)
{
	if (isCPU_)
		calc_Holo_CPU(frame);
	else
		calc_Holo_GPU(frame);
}

/**
* @brief Assign random phase value if RANDOM_PHASE == 1
* @details If RANDOM_PHASE == 1, calculate a random phase value using random generator;
*  otherwise, random phase value is 1.
* @param rand_phase_val : Input & Ouput value.
*/
void ophDepthMap::get_rand_phase_value(oph::Complex<real>& rand_phase_val)
{
	if (dm_params_.RANDOM_PHASE)
	{
		std::default_random_engine generator;
		std::uniform_real_distribution<real> distribution(0.0, 1.0);

		rand_phase_val.re = 0.0;
		rand_phase_val.im = 2 * PI * distribution(generator);
		exponent_complex(&rand_phase_val);

	} else {
		rand_phase_val.re = 1.0;
		rand_phase_val.im = 0.0;
	}
}

/**
* @brief Write the result image.
* @param ftr : the frame number of the image.
*/

void ophDepthMap::writeResultimage(int ftr)
{	
	std::string outdir = std::string("./").append(dm_params_.RESULT_FOLDER);

	if (!CreateDirectory(outdir.c_str(), NULL) && ERROR_ALREADY_EXISTS != GetLastError())
	{
		LOG("Fail to make output directory\n");
		return;
	}
	
	std::string fname = std::string("./").append(dm_params_.RESULT_FOLDER).append("/").append(dm_params_.RESULT_PREFIX).append(std::to_string(ftr)).append(".bmp");

	int pnx = context_.pixel_number[0];
	int pny = context_.pixel_number[1];
	int px = static_cast<int>(pnx / 3);
	int py = pny;

	real min_val, max_val;
	min_val = ((real*)((real*)p_hologram))[0];
	max_val = ((real*)p_hologram)[0];
	for (int i = 0; i < pnx*pny; ++i)
	{
		if (min_val > ((real*)p_hologram)[i])
			min_val = ((real*)p_hologram)[i];
		else if (max_val < ((real*)p_hologram)[i])
			max_val = ((real*)p_hologram)[i];
	}

	uchar* data = (uchar*)malloc(sizeof(uchar)*pnx*pny);
	memset(data, 0, sizeof(uchar)*pnx*pny);

	int x = 0;
#pragma omp parallel for private(x)
	for (x = 0; x < pnx*pny; ++x)
		data[x] = (uint)((((real*)p_hologram)[x] - min_val) / (max_val - min_val) * 255);

	int ret = Openholo::saveAsImg(fname.c_str(), 24, data, px, py);

	free(data);
}

/**
* @brief It is a testing function used for the reconstruction.
*/

void ophDepthMap::writeSimulationImage(int num, real val)
{
	std::string outdir = std::string("./").append(dm_params_.RESULT_FOLDER);
	if (!CreateDirectory(outdir.c_str(), NULL) && ERROR_ALREADY_EXISTS != GetLastError())
	{
		LOG("Fail to make output directory\n");
		return;
	}

	std::string fname = std::string("./").append(dm_params_.RESULT_FOLDER).append("/").append(dm_simuls_.Simulation_Result_File_Prefix_).append("_");
	fname = fname.append(dm_params_.RESULT_PREFIX).append(std::to_string(num)).append("_").append(dm_simuls_.sim_type_ == 0 ? "FOCUS_" : "EYE_Y_");
	int v = (int)round(val * 1000);
	fname = fname.append(std::to_string(v));
	
	int pnx = context_.pixel_number[0];
	int pny = context_.pixel_number[1];
	int px = pnx / 3;
	int py = pny;

	real min_val, max_val;
	min_val = dm_simuls_.sim_final_[0];
	max_val = dm_simuls_.sim_final_[0];
	for (int i = 0; i < pnx*pny; ++i)
	{
		if (min_val > dm_simuls_.sim_final_[i])
			min_val = dm_simuls_.sim_final_[i];
		else if (max_val < dm_simuls_.sim_final_[i])
			max_val = dm_simuls_.sim_final_[i];
	}

	uchar* data = (uchar*)malloc(sizeof(uchar)*pnx*pny);
	memset(data, 0, sizeof(uchar)*pnx*pny);
		
	for (int k = 0; k < pnx*pny; k++)
		data[k] = (uint)((dm_simuls_.sim_final_[k] - min_val) / (max_val - min_val) * 255);

	//QImage img(data, px, py, QImage::Format::Format_RGB888);
	//img.save(QString(fname));

	int ret = Openholo::saveAsImg(fname.c_str(), 24, data, px, py);

	free(data);

}

void ophDepthMap::ophFree(void)
{
}