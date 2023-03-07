//==============================================================================
// Alessandro de Oliveira Faria (A.K.A.CABELO)
// Apache-2.0 license  
//==============================================================================
/// For more information see
/// https://github.com/cabelo/oneapi-antispoofing
/// @file

#define __STDC_WANT_LIB_EXT1__ 1
#include <string.h> 
#include <memory.h>  
#include <stdio.h>  

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "vpl/mfxvideo.h"

#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/dnn.hpp"
#include "opencv2/imgproc/hal/hal.hpp"

#define MAX_PATH    260
#include "common.hpp"
#include "util.h"

#define BITSTREAM_BUFFER_SIZE      2000000
#define MAJOR_API_VERSION_REQUIRED 2
#define MINOR_API_VERSION_REQUIRED 2

int _last_left;
int _last_top;
int _last_right;
int _last_bottom;
int _show;

mfxStatus ReadEncodedStream(mfxBitstream &bs, mfxU32 codecid, FILE *f);
void WriteRawFrame(mfxFrameSurface1 *pSurface, FILE *f);
mfxU32 GetSurfaceSize(mfxU32 FourCC, mfxU32 width, mfxU32 height);
int GetFreeSurfaceIndex(mfxFrameSurface1 *SurfacesPool, mfxU16 nPoolSize);
char *ValidateFileName(char *in);
void Usage(void);
cv::Mat Frame(mfxFrameSurface1 *s);
void drawFraud(int left, int top, int right, int bottom, cv::Mat& frame);
void postprocess(cv::Mat& frame, const std::vector<cv::Mat>& outs, cv::dnn::Net& net,float _confThreshold, float _nmsThreshold,  std::vector<std::string> _classes);
void drawOk(int left, int top, int right, int bottom, cv::Mat& frame);

int main(int argc, char *argv[]) {
    _show = 0;
    if (argc < 2 || argc>3) {
        Usage();
        return 1;
    }

    if (argc == 3)
    {
        _show = 1;
    }

    char *in_filename = NULL;

    in_filename = ValidateFileName(argv[1]);
    if (!in_filename) {
        printf("Input filename is not valid\n");
        Usage();
        return 1;
    }

    FILE *fSource = fopen(in_filename, "rb");

    if (!fSource) {
        printf("could not open input file, %s\n", in_filename);
        return 1;
    }

    _last_left = 0;
    _last_top = 0;
    _last_right = 0;
    _last_bottom = 0;

    bool isFailed = false;
    int inpWidth = 416;
    int inpHeight = 416;
    float confThreshold = 0.25;
    float nmsThreshold = 0.40;
    //float scale = 0.00392;
    float scale = 0.01;
    std::string modelPath = "antispoofing.weights";
    std::string configPath = "antispoofing.cfg";
    std::vector<std::string> classes;
    classes.push_back("FRAUDE_01");
    classes.push_back("FRAUDE_02");
    cv::dnn::Net net = cv::dnn::readNet(modelPath, configPath);
    std::vector<String> outNames = net.getUnconnectedOutLayersNames();

    // variables used only in 2.x version
    mfxConfig cfg[3];
    mfxVariant cfgVal[3];
    mfxLoader loader = NULL;
    mfxBitstream bitstream          = {};
    mfxVideoParam decodeParams      = {};
    mfxStatus sts                   = MFX_ERR_NONE;
    mfxSession session              = NULL;


    // Initialize VPL session
    loader = MFXLoad();
    VERIFY(NULL != loader, "MFXLoad failed -- is implementation in path?");

    // Implementation used must be the type requested from command line
    cfg[0] = MFXCreateConfig(loader);
    VERIFY(NULL != cfg[0], "MFXCreateConfig failed")

    // Implementation must provide an HEVC decoder
    cfg[1] = MFXCreateConfig(loader);
    VERIFY(NULL != cfg[1], "MFXCreateConfig failed")
    cfgVal[1].Type     = MFX_VARIANT_TYPE_U32;
    cfgVal[1].Data.U32 = MFX_CODEC_HEVC;
    sts                = MFXSetConfigFilterProperty(
        cfg[1],
        (mfxU8 *)"mfxImplDescription.mfxDecoderDescription.decoder.CodecID",
        cfgVal[1]);
    VERIFY(MFX_ERR_NONE == sts, "MFXSetConfigFilterProperty failed for decoder CodecID");

    // Implementation used must provide API version 2.2 or newer
    cfg[2] = MFXCreateConfig(loader);
    VERIFY(NULL != cfg[2], "MFXCreateConfig failed")
    cfgVal[2].Type     = MFX_VARIANT_TYPE_U32;
    cfgVal[2].Data.U32 = VPLVERSION(MAJOR_API_VERSION_REQUIRED, MINOR_API_VERSION_REQUIRED);
    sts                = MFXSetConfigFilterProperty(cfg[2],
                                     (mfxU8 *)"mfxImplDescription.ApiVersion.Version",
                                     cfgVal[2]);
    VERIFY(MFX_ERR_NONE == sts, "MFXSetConfigFilterProperty failed for API version");

    sts = MFXCreateSession(loader, 0, &session);
    VERIFY(MFX_ERR_NONE == sts,
           "Cannot create session -- no implementations meet selection criteria");


    // Print info about implementation loaded
    ShowImplementationInfo(loader, 0);

    // Prepare input bitstream and start decoding
    bitstream.MaxLength = BITSTREAM_BUFFER_SIZE;
    bitstream.Data      = (mfxU8 *)calloc(bitstream.MaxLength, sizeof(mfxU8));
    VERIFY(bitstream.Data, "Not able to allocate input buffer");
    bitstream.CodecId = MFX_CODEC_HEVC;

    // Pre-parse input stream
    sts = ReadEncodedStream(bitstream, fSource);
    VERIFY(MFX_ERR_NONE == sts, "Error reading bitstream\n");

    decodeParams.mfx.CodecId = MFX_CODEC_HEVC;
    decodeParams.IOPattern   = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    sts                      = MFXVideoDECODE_DecodeHeader(session, &bitstream, &decodeParams);
    VERIFY(MFX_ERR_NONE == sts, "Error decoding header\n");
    
    mfxU32 codecID = MFX_CODEC_HEVC;

    mfxFrameAllocRequest DecRequest = { 0 };
    MFXVideoDECODE_QueryIOSurf(session, &decodeParams, &DecRequest);

    mfxU16 nSurfNumDec = DecRequest.NumFrameSuggested;

    std::vector<mfxFrameSurface1> decode_surfaces;
    decode_surfaces.resize(nSurfNumDec);
    mfxFrameSurface1 *decSurfaces = decode_surfaces.data();
    if (decSurfaces == NULL) {
        fclose(fSource);
        //fclose(fSink);
        puts("Fail to allocate decode frame memory surface");
        return 1;
    }

    // initialize surface pool for decode (I420 format)
    mfxU32 surfaceSize = GetSurfaceSize(decodeParams.mfx.FrameInfo.FourCC,
                                        decodeParams.mfx.FrameInfo.Width,
                                        decodeParams.mfx.FrameInfo.Height);
    if (surfaceSize == 0) {
        fclose(fSource);
        //fclose(fSink);
        puts("Surface size is wrong");
        return 1;
    }
    size_t framePoolBufSize = static_cast<size_t>(surfaceSize) * nSurfNumDec;
    std::vector<mfxU8> output_buffer;
    output_buffer.resize(framePoolBufSize);
    mfxU8 *DECoutbuf = output_buffer.data();

    mfxU16 surfW = (decodeParams.mfx.FrameInfo.FourCC == MFX_FOURCC_I010)
                       ? decodeParams.mfx.FrameInfo.Width * 2
                       : decodeParams.mfx.FrameInfo.Width;
    mfxU16 surfH = decodeParams.mfx.FrameInfo.Height;

    for (mfxU32 i = 0; i < nSurfNumDec; i++) {
        decSurfaces[i]        = { 0 };
        decSurfaces[i].Info   = decodeParams.mfx.FrameInfo;
        size_t buf_offset     = static_cast<size_t>(i) * surfaceSize;
        decSurfaces[i].Data.Y = DECoutbuf + buf_offset;
        decSurfaces[i].Data.U = DECoutbuf + buf_offset + (surfW * surfH);
        decSurfaces[i].Data.V =
            decSurfaces[i].Data.U + ((surfW / 2) * (surfH / 2));
        decSurfaces[i].Data.Pitch = surfW;
    }

    sts = MFXVideoDECODE_Init(session, &decodeParams);
    VERIFY(MFX_ERR_NONE == sts, "Error initializing decode\n");

    printf("Output colorspace: ");
    switch (decodeParams.mfx.FrameInfo.FourCC) {
        case MFX_FOURCC_I420: // CPU output
            printf("I420 (aka yuv420p)\n");
            break;
        case MFX_FOURCC_NV12: // GPU output
            printf("NV12\n");
            break;
        default:
            printf("Unsupported color format\n");
            isFailed = true;
            //goto end;
            exit(1);
            break;
    }
	
    // ------------------
    // main loop
    // ------------------
    int framenum                     = 0;
    mfxSyncPoint syncp               = { 0 };
    mfxFrameSurface1 *pmfxOutSurface = nullptr;
    cv::Mat _frame;

    for (;;) {
        bool stillgoing = true;
        int nIndex      = GetFreeSurfaceIndex(decSurfaces, nSurfNumDec);
        while (stillgoing) {
            // submit async decode request
            sts = MFXVideoDECODE_DecodeFrameAsync(session, &bitstream, &decSurfaces[nIndex], &pmfxOutSurface, &syncp);

            // next step actions provided by application
            switch (sts) {
                case MFX_ERR_MORE_DATA: // more data is needed to decode
                    ReadEncodedStream(bitstream, codecID, fSource);
                    if (bitstream.DataLength == 0)
                        stillgoing = false; // stop if end of file
                    break;
                case MFX_ERR_MORE_SURFACE: // feed a fresh surface to decode
                    nIndex = GetFreeSurfaceIndex(decSurfaces, nSurfNumDec);
                    break;
                case MFX_ERR_NONE: // no more steps needed, exit loop
                    stillgoing = false;
                    break;
                default: // state is not one of the cases above
                    printf("Error in DecodeFrameAsync: sts=%d\n", sts);
                    exit(1);
                    break;
            }
        }

        if (sts < 0)
            break;
        // data available to app only after sync
        MFXVideoCORE_SyncOperation(session, syncp, 60000);

 	_frame = Frame(pmfxOutSurface);

        framenum++;
        printf("%d ", framenum);

	cv::Mat blob;
	cv::Size inpSize(inpWidth > 0 ? inpWidth : _frame.cols, inpHeight > 0 ? inpHeight : _frame.rows);
	cv::dnn::blobFromImage(_frame, blob, scale, inpSize, cv::Scalar(), false, false);

        net.setInput(blob);
        std::vector<cv::Mat> outs;
        net.forward(outs, outNames);
	postprocess(_frame, outs, net, confThreshold, nmsThreshold,  classes);

        cv::waitKey(3);


    }

    sts = MFX_ERR_NONE;
    memset(&syncp, 0, sizeof(mfxSyncPoint));
    pmfxOutSurface = nullptr;

    // retrieve the buffered decoded frames
    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_SURFACE == sts) {
        int nIndex =
            GetFreeSurfaceIndex(decSurfaces,
                                nSurfNumDec); // Find free frame surface

        // Decode a frame asychronously (returns immediately)

        sts = MFXVideoDECODE_DecodeFrameAsync(session,
                                              NULL,
                                              &decSurfaces[nIndex],
                                              &pmfxOutSurface,
                                              &syncp);

        // Ignore warnings if output is available,
        // if no output and no action required just repeat the DecodeFrameAsync call
        if (MFX_ERR_NONE < sts && syncp)
            sts = MFX_ERR_NONE;

        if (sts == MFX_ERR_NONE) {
            sts = MFXVideoCORE_SyncOperation(
                session,
                syncp,
                60000); // Synchronize. Waits until decoded frame is ready
            _frame = Frame(pmfxOutSurface);

            framenum++;
            printf("%d ", framenum);
	    cv::Mat blob;
            cv::Size inpSize(inpWidth > 0 ? inpWidth : _frame.cols, inpHeight > 0 ? inpHeight : _frame.rows);
	    cv::dnn::blobFromImage(_frame, blob, scale, inpSize, cv::Scalar(), false, false);

            net.setInput(blob);
            std::vector<cv::Mat> outs;
            net.forward(outs, outNames);
	    postprocess(_frame, outs, net, confThreshold, nmsThreshold,  classes);
            cv::waitKey(3);
        }
    }

    printf("Decoded %d frames\n", framenum);

    fclose(fSource);
    MFXVideoDECODE_Close(session);

    return 0;
}

// Read encoded stream from file
mfxStatus ReadEncodedStream(mfxBitstream &bs, mfxU32 codecid, FILE *f) {
    memmove(bs.Data, bs.Data + bs.DataOffset, bs.DataLength);
    bs.DataLength = static_cast<mfxU32>(fread(bs.Data, 1, bs.MaxLength, f));
    return MFX_ERR_NONE;
}

// Return the surface size in bytes given format and dimensions
mfxU32 GetSurfaceSize(mfxU32 FourCC, mfxU32 width, mfxU32 height) {
    mfxU32 nbytes = 0;
    switch (FourCC) {
        case MFX_FOURCC_IYUV:
            nbytes = width * height + (width >> 1) * (height >> 1) +
                     (width >> 1) * (height >> 1);
            break;
        default:
            break;
    }
    return nbytes;
}

// Return index of free surface in given pool
int GetFreeSurfaceIndex(mfxFrameSurface1 *SurfacesPool, mfxU16 nPoolSize) {
    for (mfxU16 i = 0; i < nPoolSize; i++) {
        if (0 == SurfacesPool[i].Data.Locked)
            return i;
    }
    return MFX_ERR_NOT_FOUND;
}

char *ValidateFileName(char *in) {
    if (in) {
        if (strlen(in) > MAX_PATH)
            return NULL;
    }

    return in;
}

// Print usage message
void Usage(void) {
    printf("Usage: antispoofinge [SOURCE-VIDEO-FILE.h265] y\n");
}

cv::Mat Frame(mfxFrameSurface1 *s)
{
    mfxFrameInfo &info = s->Info;
    mfxFrameData &data = s->Data;
    
    mfxU8 *pOutData = new mfxU8[s->Info.BufferSize]; 
    mfxU32 i, j, h, w;
    for (int i = 0; i < s->Info.CropH; i++) 
    {
        memcpy(pOutData + (i*s->Info.CropW),  s->Data.Y + (s->Info.CropY * s->Data.Pitch + s->Info.CropX) + i * s->Data.Pitch, s->Info.CropW);
    }

    h = s->Info.CropH / 2;
    w = s->Info.CropW;
    for (i = 0; i < h; i++)
        for (j = 0; j < w; j += 2) 
        {
            memcpy(pOutData + (s->Info.CropH * s->Info.CropW) + (j + (i*s->Info.CropW)), s->Data.UV + (s->Info.CropY * s->Data.Pitch / 2 + s->Info.CropX) + i * s->Data.Pitch + j, 1);
        }

    for (i = 0; i < h; i++)
        for (j = 1; j < w; j += 2) 
        {
            memcpy(pOutData + (s->Info.CropH * s->Info.CropW) + (j + (i*s->Info.CropW)), s->Data.UV + (s->Info.CropY * s->Data.Pitch / 2 + s->Info.CropX) + i * s->Data.Pitch + j, 1);
        }

    cv::Mat picYV12 = cv::Mat(s->Info.CropH + (s->Info.CropH / 2), s->Info.CropW, CV_8UC1, pOutData);
    cv::Mat picBGR;
    cv::cvtColor(picYV12, picBGR, cv::COLOR_YUV2BGR_I420);
    delete pOutData;

    return picBGR;
}

void postprocess(cv::Mat& frame, const std::vector<cv::Mat>& outs, cv::dnn::Net& net,float _confThreshold, float _nmsThreshold,  std::vector<std::string> _classes)
{
    static std::vector<int> outLayers = net.getUnconnectedOutLayers();
    static std::string outLayerType = net.getLayer(outLayers[0])->type;

    std::vector<int> classIds;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;
    if (outLayerType == "Region")
    {
        for (size_t i = 0; i < outs.size(); ++i)
        {
            // Network produces output blob with a shape NxC where N is a number of
            // detected objects and C is a number of classes + 4 where the first 4
            // numbers are [center_x, center_y, width, height]
            float* data = (float*)outs[i].data;
            for (int j = 0; j < outs[i].rows; ++j, data += outs[i].cols)
            {
		    cv::Mat scores = outs[i].row(j).colRange(5, outs[i].cols);
		    cv::Point classIdPoint;
                double confidence;
		cv::minMaxLoc(scores, 0, &confidence, 0, &classIdPoint);
                if (confidence > _confThreshold)
                {
                    int centerX = (int)(data[0] * frame.cols);
                    int centerY = (int)(data[1] * frame.rows);
                    int width = (int)(data[2] * frame.cols);
                    int height = (int)(data[3] * frame.rows);
                    int left = centerX - width / 2;
                    int top = centerY - height / 2;

                    classIds.push_back(classIdPoint.x);
                    confidences.push_back((float)confidence);
                    boxes.push_back(cv::Rect(left, top, width, height));
                }
            }
        }
    }
    else
        CV_Error(Error::StsNotImplemented, "Unknown output layer type: " + outLayerType);

    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, _confThreshold, _nmsThreshold, indices);
    for (size_t i = 0; i < indices.size(); ++i)
    {
        int idx = indices[i];
	cv::Rect box = boxes[idx];
        drawFraud( box.x, box.y, box.x + box.width, box.y + box.height, frame);
	_last_top = box.y;
        _last_left = box.x;
        _last_right = box.x + box.width;
        _last_bottom = box.y + box.height;

    }

    if(indices.size() == 0 && _last_top != 0)
    {
	    drawFraud(_last_left,_last_top,_last_right,_last_bottom, frame);
    }
    else
    {
	if(_last_top == 0)
	{
    	    drawOk(_last_left,_last_top,_last_right,_last_bottom, frame);
	}
       
    }

    if(_show)
    {
        cv::imshow("Display decoded output", frame);
    }

}


void drawFraud(int left, int top, int right, int bottom, cv::Mat& frame)
{
    if(_show)
    {	    
        cv::putText(frame, "FRAUD", cv::Point(10,50), FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0,0,255));
        cv::line(frame,cv::Point(left, top),cv::Point(right, bottom), cv::Scalar(0,0,255),3);
        cv::line(frame,cv::Point(left, bottom),cv::Point(right, top), cv::Scalar(0,0,255),3);
    }
    printf("FRAUD! \n");
}

void drawOk(int left, int top, int right, int bottom, cv::Mat& frame)
{
    if(_show)
    {
        cv::putText(frame, "OK", cv::Point(10,50), FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0,255,0));
    }
    printf("OK! \n");
}
