#include "avisynth.h"
#include "butteraugli/butteraugli.h"

static void ScoreToRgb(double score, double good_threshold, double bad_threshold, uint8_t& r, uint8_t& g, uint8_t& b)
{
    double heatmap[12][3] =
    {
        { 0, 0, 0 },
        { 0, 0, 1 },
        { 0, 1, 1 },
        { 0, 1, 0 }, // Good level
        { 1, 1, 0 },
        { 1, 0, 0 }, // Bad level
        { 1, 0, 1 },
        { 0.5, 0.5, 1.0 },
        { 1.0, 0.5, 0.5 },  // Pastel colors for the very bad quality range.
        { 1.0, 1.0, 0.5 },
        { 1, 1, 1, },
        { 1, 1, 1, },
    };

    if (score < good_threshold)
        score = (score / good_threshold) * 0.3;
    else if (score < bad_threshold)
        score = 0.3 + (score - good_threshold) / (bad_threshold - good_threshold) * 0.15;
    else
        score = 0.45 + (score - bad_threshold) / (bad_threshold * 12) * 0.5;

    static const int kTableSize = sizeof(heatmap) / sizeof(heatmap[0]);

    score = std::min<double>(std::max<double>(score * ((__int64)kTableSize - 1), 0.0), (__int64)kTableSize - 2);

    int ix = static_cast<int>(score);

    double mix = score - ix;

    //r
    double v = mix * heatmap[ix + 1][0] + (1 - mix) * heatmap[ix][0];
    r = static_cast<uint8_t>(255 * pow(v, 0.5) + 0.5);

    //g
    v = mix * heatmap[ix + 1][1] + (1 - mix) * heatmap[ix][1];
    g = static_cast<uint8_t>(255 * pow(v, 0.5) + 0.5);

    //b
    v = mix * heatmap[ix + 1][2] + (1 - mix) * heatmap[ix][2];
    b = static_cast<uint8_t>(255 * pow(v, 0.5) + 0.5);
}

static void WriteResult(const butteraugli::ImageF& distmap, double good_threshold, double bad_threshold, size_t xsize, size_t ysize, uint8_t* dst_r, uint8_t* dst_g, uint8_t* dst_b, int stride)
{
    for (size_t y = 0; y < ysize; ++y)
    {
        for (size_t x = 0; x < xsize; ++x)
        {
            ScoreToRgb(distmap.Row(y)[x], good_threshold, bad_threshold, dst_r[x], dst_g[x], dst_b[x]);
        }

        dst_r += stride;
        dst_g += stride;
        dst_b += stride;
    }
}

static void FromSrgbToLinear(const std::vector<butteraugli::Image8>& rgb, std::vector<butteraugli::ImageF>& linear, double* kSrgbToLinearTable, int background)
{
    const size_t xsize = rgb[0].xsize();
    const size_t ysize = rgb[0].ysize();

    for (int i = 0; i < 256; ++i)
    {
        const double srgb = i / 255.0;
        kSrgbToLinearTable[i] = 255.0 * (srgb <= 0.04045 ? srgb / 12.92 : std::pow((srgb + 0.055) / 1.055, 2.4));
    }

    for (int c = 0; c < 3; ++c)  //first for loop
    {
        linear.push_back(butteraugli::ImageF(xsize, ysize));
        for (size_t y = 0; y < ysize; ++y) //second for loop
        {
            const uint8_t* const BUTTERAUGLI_RESTRICT row_rgb = rgb[c].Row(y);
            float* const BUTTERAUGLI_RESTRICT row_linear = linear[c].Row(y);
            for (size_t x = 0; x < xsize; ++x) //third for loop
            {
                row_linear[x] = kSrgbToLinearTable[row_rgb[x]];
            } //end third for loop
        } //end second for loop
    } //end first for loop
} //end FromSrgbToLinear

class butteraugli_ : public GenericVideoFilter
{
    PClip clip;
    bool heatmap_;
    double* table;

public:
    butteraugli_(PClip _child, PClip _clip, bool heatmap, IScriptEnvironment* env)
        : GenericVideoFilter(_child), clip(_clip), heatmap_(heatmap)
    {
        if ((!vi.IsPlanarRGB() && !vi.IsPlanarRGBA()) || vi.BitsPerComponent() > 8)
            env->ThrowError("butteraugli: only planar RGB 8-bit clip supported");

        const VideoInfo vi1 = clip->GetVideoInfo();

        if (vi.height != vi1.height || vi.width != vi1.width)
            env->ThrowError("butteraugli: both clips must have same dimensions.");

        table = new double[256];
    }

    ~butteraugli_()
    {
        delete[] table;
    }
    
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env)
    {
        PVideoFrame src1 = child->GetFrame(n, env);
        PVideoFrame src2 = clip->GetFrame(n, env);

        const int height = src1->GetHeight();
        const int width = src1->GetRowSize();

        std::vector<butteraugli::Image8> rgb1(butteraugli::CreatePlanes<uint8_t>(width, height, 3));
        std::vector<butteraugli::Image8> rgb2(butteraugli::CreatePlanes<uint8_t>(width, height, 3));

        int planes_r[3] = { PLANAR_R, PLANAR_G, PLANAR_B };

        for (int pid = 0; pid < 3; ++pid)
        {
            const int plane = planes_r[pid];

            const uint8_t* srcp1 = src1->GetReadPtr(plane);
            const uint8_t* srcp2 = src2->GetReadPtr(plane);
            const int src_stride1 = src1->GetPitch(plane);
            const int src_stride2 = src2->GetPitch(plane);

            for (int y = 0; y < height; ++y)
            {
                memcpy(rgb1.at(pid).Row(y), srcp1, width * sizeof(uint8_t));
                memcpy(rgb2.at(pid).Row(y), srcp2, width * sizeof(uint8_t));
                srcp1 += src_stride1;
                srcp2 += src_stride2;
            }
        }

        std::vector<butteraugli::ImageF> linear1, linear2;
        FromSrgbToLinear(rgb1, linear1, table, 0);
        FromSrgbToLinear(rgb2, linear2, table, 0);

        double diff_value;
        butteraugli::ImageF diff_map;

        if (!ButteraugliInterface(linear1, linear2, 1.0, diff_map, diff_value))
            env->ThrowError("butteraugli: Fail to compare.");

        PVideoFrame dst = env->NewVideoFrame(vi);

        if (heatmap_)
        {
            WriteResult(diff_map, butteraugli::ButteraugliFuzzyInverse(1.5), butteraugli::ButteraugliFuzzyInverse(0.5), rgb1[0].xsize(), rgb2[0].ysize(), dst->GetWritePtr(PLANAR_R), dst->GetWritePtr(PLANAR_G), dst->GetWritePtr(PLANAR_B), dst->GetPitch());
            env->copyFrameProps(src2, dst);
        }
        else
            dst = src2;
        
        env->propSetFloat(env->getFramePropsRW(dst), "_FrameButteraugli", diff_value, 0);

        return dst;
    }

    int __stdcall SetCacheHints(int cachehints, int frame_range)
    {
        return cachehints == CACHE_GET_MTMODE ? MT_MULTI_INSTANCE : 0;
    }
};

AVSValue __cdecl Create_butteraugli_(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    return new butteraugli_(
        args[0].AsClip(),
        args[1].AsClip(),
        args[2].AsBool(true),
        env);
}

const AVS_Linkage* AVS_linkage;

extern "C" __declspec(dllexport)
const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* env, const AVS_Linkage* const vectors)
{
    AVS_linkage = vectors;

    env->AddFunction("Butteraugli", "cc[heatmap]b", Create_butteraugli_, 0);
    return "Butteraugli";
}
