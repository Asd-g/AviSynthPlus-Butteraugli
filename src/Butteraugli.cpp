#include "avisynth.h"
#include "butteraugli/butteraugli.h"

template <typename T>
static void ScoreToRgb(T& r, T& g, T& b, double score, double good_threshold, double bad_threshold, int peak)
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

    score = std::min<double>(std::max<double>(score * (static_cast<int64_t>(kTableSize) - 1), 0.0), static_cast<int64_t>(kTableSize) - 2);

    int ix = static_cast<int>(score);

    double mix = score - ix;

    //r
    double vr = mix * heatmap[ix + 1][0] + (1 - mix) * heatmap[ix][0];
    //g
    double vg = mix * heatmap[ix + 1][1] + (1 - mix) * heatmap[ix][1];
    //b
    double vb = mix * heatmap[ix + 1][2] + (1 - mix) * heatmap[ix][2];    

    if constexpr (std::is_integral_v<T>)
    {
        r = static_cast<T>(peak * pow(vr, 0.5) + 0.5);
        g = static_cast<T>(peak * pow(vg, 0.5) + 0.5);
        b = static_cast<T>(peak * pow(vb, 0.5) + 0.5);
    }
    else
    {
        r = static_cast<float>(pow(vr, 0.5));
        g = static_cast<float>(pow(vg, 0.5));
        b = static_cast<float>(pow(vb, 0.5));
    }

}

template <typename T>
static void WriteResult(const butteraugli::ImageF& distmap, PVideoFrame& dst, double good_threshold, double bad_threshold, int peak)
{
    const int pixel_size = sizeof(T);
    const int stride = dst->GetPitch() / pixel_size;
    const int height = dst->GetHeight();
    const int width = dst->GetRowSize() / pixel_size;
    T* dst_r = reinterpret_cast<T*>(dst->GetWritePtr(PLANAR_R));
    T* dst_g = reinterpret_cast<T*>(dst->GetWritePtr(PLANAR_G));
    T* dst_b = reinterpret_cast<T*>(dst->GetWritePtr(PLANAR_B));

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
            ScoreToRgb<T>(dst_r[x], dst_g[x], dst_b[x], distmap.Row(y)[x], good_threshold, bad_threshold, peak);

        dst_r += stride;
        dst_g += stride;
        dst_b += stride;
    }
}

template <typename T>
static void load(std::vector<butteraugli::ImageF>& linear, PVideoFrame& src, int peak)
{
    const int pixel_size = sizeof(T);
    const int float_size = sizeof(float);
    const int stride = src->GetPitch() / pixel_size;
    const int height = src->GetHeight();
    const int width = src->GetRowSize() / pixel_size;
    linear = std::vector<butteraugli::ImageF>(butteraugli::CreatePlanes<float>(width, height, 3));
    int planes[3] = { PLANAR_R, PLANAR_G, PLANAR_B };
    float* p = static_cast<float*>(_aligned_malloc(static_cast<int64_t>(stride) * height * float_size, 32));

    if constexpr (std::is_same_v<T, uint8_t>)
    {
        for (int i = 0; i < 3; ++i)
        {
            const int plane = planes[i];
            const uint8_t* srcp = src->GetReadPtr(plane);

            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                    p[x] = srcp[x];

                memcpy(linear[i].Row(y), p, static_cast<int64_t>(width) * float_size);

                srcp += stride;
            }
        }
    }
    else if constexpr (std::is_same_v<T, uint16_t>)
    {
        for (int i = 0; i < 3; ++i)
        {
            const int plane = planes[i];
            const uint16_t* srcp = reinterpret_cast<const uint16_t*>(src->GetReadPtr(plane));

            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                    p[x] = srcp[x] / static_cast<float>(peak) * 255.0f;

                memcpy(linear[i].Row(y), p, static_cast<int64_t>(width) * float_size);

                srcp += stride;
            }
        }
    }
    else
    {
        for (int i = 0; i < 3; ++i)
        {
            const int plane = planes[i];
            const float* srcp = reinterpret_cast<const float*>(src->GetReadPtr(plane));

            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                    p[x] = srcp[x] * 255.0f;

                memcpy(linear[i].Row(y), p, static_cast<int64_t>(width) * pixel_size);

                srcp += stride;
            }
        }
    }

    _aligned_free(p);
}

static void convert_to_linear(std::vector<butteraugli::ImageF>& linear)
{
    const int height = linear[0].ysize();
    const int width = linear[0].xsize();
    double kSrgbToLinearTable[256]{ 0.0 };

    for (int i = 0; i < 256; ++i)
    {
        const double srgb = i / 255.0;
        kSrgbToLinearTable[i] = 255.0 * (srgb <= (12.92 * 0.003041282560128) ? srgb / 12.92 : std::pow((srgb + 0.055010718947587) / 1.055010718947587, 2.4));
    }

    for (int i = 0; i < 3; ++i)
    {
        for (int y = 0; y < height; ++y)
        {
            const float* const BUTTERAUGLI_RESTRICT rgb_from = linear[i].Row(y);
            float* const BUTTERAUGLI_RESTRICT rgbf_to = linear[i].Row(y);

            for (int x = 0; x < width; ++x)
                rgbf_to[x] = kSrgbToLinearTable[llrintf(rgb_from[x])];
        }
    }
}

class butteraugli_ : public GenericVideoFilter
{
    PClip clip;
    float hf_asymmetry_;
    bool heatmap_, linput_;

    void (*load_)(std::vector<butteraugli::ImageF>&, PVideoFrame&, int);
    void (*hmap)(const butteraugli::ImageF&, PVideoFrame&, double, double, int);

public:
    butteraugli_(PClip _child, PClip _clip, float hf_asymmetry, bool heatmap, bool linput, IScriptEnvironment* env)
        : GenericVideoFilter(_child), clip(_clip), hf_asymmetry_(hf_asymmetry), heatmap_(heatmap), linput_(linput)
    {
        if (!vi.IsPlanarRGB() && !vi.IsPlanarRGBA())
            env->ThrowError("Butteraugli: the clip must be in RGB planar format.");

        const VideoInfo vi1 = clip->GetVideoInfo();

        if (vi.height != vi1.height || vi.width != vi1.width)
            env->ThrowError("Butteraugli: the clips must have same dimensions.");
        if (!vi.IsSameColorspace(vi1))
            env->ThrowError("Butteraugli: the clips must be of the same color space.");
        if (hf_asymmetry_ <= 0.0f)
            env->ThrowError("Butteraugli: hf_asymmetry must be greater than 0.0");

        switch (vi.ComponentSize())
        {
            case 1:
                load_ = load<uint8_t>;
                hmap = WriteResult<uint8_t>;
                break;
            case 2:
                load_ = load<uint16_t>;
                hmap = WriteResult<uint16_t>;
                break;
            default:
                load_ = load<float>;
                hmap = WriteResult<float>;
                break;
        }
    }

    int __stdcall SetCacheHints(int cachehints, int frame_range)
    {
        return cachehints == CACHE_GET_MTMODE ? MT_MULTI_INSTANCE : 0;
    }

    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env)
    {
        PVideoFrame src1 = child->GetFrame(n, env);
        PVideoFrame src2 = clip->GetFrame(n, env);
        std::vector<butteraugli::ImageF> linear1, linear2;
        const int peak = (1 << vi.BitsPerComponent()) - 1;

        load_(linear1, src1, peak);
        load_(linear2, src2, peak);

        if (!linput_)
        {
            convert_to_linear(linear1);
            convert_to_linear(linear2);
        }

        double diff_value;
        butteraugli::ImageF diff_map;

        if (!ButteraugliInterface(linear1, linear2, hf_asymmetry_, diff_map, diff_value))
            env->ThrowError("butteraugli: fail to compare.");

        PVideoFrame dst = env->NewVideoFrame(vi);

        if (heatmap_)
        {
            hmap(diff_map, dst, butteraugli::ButteraugliFuzzyInverse(1.5), butteraugli::ButteraugliFuzzyInverse(0.5), peak);
            env->copyFrameProps(src2, dst);
        }
        else
            dst = src2;

        env->propSetFloat(env->getFramePropsRW(dst), "_FrameButteraugli", diff_value, 0);

        return dst;
    }
};

AVSValue __cdecl Create_butteraugli_(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    return new butteraugli_(
        args[0].AsClip(),
        args[1].AsClip(),
        args[2].AsFloatf(1.0f),
        args[3].AsBool(true),
        args[4].AsBool(false),
        env);
}

const AVS_Linkage* AVS_linkage;

extern "C" __declspec(dllexport)
const char* __stdcall AvisynthPluginInit3(IScriptEnvironment * env, const AVS_Linkage* const vectors)
{
    AVS_linkage = vectors;

    env->AddFunction("Butteraugli", "cc[hf_asymmetry]f[heatmap]b[linput]b", Create_butteraugli_, 0);
    return "Butteraugli";
}
