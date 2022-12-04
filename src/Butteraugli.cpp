#include "avisynth.h"
#include "lib/jxl/enc_butteraugli_comparator.h"
#include "lib/jxl/enc_color_management.h"

template <typename T>
void heatmap(PVideoFrame& dst, const jxl::ImageF& distmap)
{
    jxl::Image3F buff = jxl::CreateHeatMapImage(distmap, jxl::ButteraugliFuzzyInverse(1.5), jxl::ButteraugliFuzzyInverse(0.5));

    const int stride = dst->GetPitch() / sizeof(T);
    const int height = dst->GetHeight();
    const int width = dst->GetRowSize() / sizeof(T);
    
    const int planes[3] = { PLANAR_R, PLANAR_G, PLANAR_B };

    if constexpr (std::is_same_v<T, uint8_t>)
    {
        jxl::Image3B tmp(width, height);
        jxl::Image3Convert(buff, 255, &tmp);

        for (int i = 0; i < 3; ++i)
        {
            uint8_t* dstp = dst->GetWritePtr(planes[i]);

            for (int y = 0; y < height; ++y)
            {
                memcpy(dstp, tmp.ConstPlaneRow(i, y), width * sizeof(T));

                dstp += stride;
            }
        }
    }
    else if constexpr (std::is_same_v<T, uint16_t>)
    {
        jxl::Image3U tmp(width, height);
        jxl::Image3Convert(buff, 65535, &tmp);

        for (int i = 0; i < 3; ++i)
        {
            uint16_t* dstp = reinterpret_cast<uint16_t*>(dst->GetWritePtr(planes[i]));

            for (int y = 0; y < height; ++y)
            {
                memcpy(dstp, tmp.ConstPlaneRow(i, y), width * sizeof(T));

                dstp += stride;
            }
        }
    }
    else
    {
        for (int i = 0; i < 3; ++i)
        {
            float* dstp = reinterpret_cast<float*>(dst->GetWritePtr(planes[i]));

            for (int y = 0; y < height; ++y)
            {
                memcpy(dstp, buff.ConstPlaneRow(i, y), width * sizeof(T));

                dstp += stride;
            }
        }
    }
}

template <typename T, bool linput>
void fill_image(jxl::CodecInOut& ref, jxl::CodecInOut& dist, PVideoFrame& src1, PVideoFrame& src2)
{
    const int stride1 = src1->GetPitch() / sizeof(T);
    const int stride2 = src2->GetPitch() / sizeof(T);
    const int height = src1->GetHeight();
    const int width = src1->GetRowSize() / sizeof(T);

    const int planes[3] = { PLANAR_R, PLANAR_G, PLANAR_B };

    if constexpr (std::is_same_v<T, uint8_t>)
    {
        jxl::Image3B tmp1(width, height);
        jxl::Image3B tmp2(width, height);

        for (int i = 0; i < 3; ++i)
        {
            const uint8_t* srcp1 = src1->GetReadPtr(planes[i]);
            const uint8_t* srcp2 = src2->GetReadPtr(planes[i]);

            for (int y = 0; y < height; ++y)
            {
                memcpy(tmp1.PlaneRow(i, y), srcp1, width * sizeof(T));
                memcpy(tmp2.PlaneRow(i, y), srcp2, width * sizeof(T));
                
                srcp1 += stride1;
                srcp2 += stride2;
            }
        }

        ref.SetFromImage(std::move(jxl::ConvertToFloat(tmp1)), (linput) ? jxl::ColorEncoding::LinearSRGB(false) : jxl::ColorEncoding::SRGB(false));
        dist.SetFromImage(std::move(jxl::ConvertToFloat(tmp2)), (linput) ? jxl::ColorEncoding::LinearSRGB(false) : jxl::ColorEncoding::SRGB(false));
    }
    else if constexpr (std::is_same_v<T, uint16_t>)
    {
        jxl::Image3U tmp1(width, height);
        jxl::Image3U tmp2(width, height);

        for (int i = 0; i < 3; ++i)
        {
            const uint16_t* srcp1 = reinterpret_cast<const uint16_t*>(src1->GetReadPtr(planes[i]));
            const uint16_t* srcp2 = reinterpret_cast<const uint16_t*>(src2->GetReadPtr(planes[i]));

            for (int y = 0; y < height; ++y)
            {
                memcpy(tmp1.PlaneRow(i, y), srcp1, width * sizeof(T));
                memcpy(tmp2.PlaneRow(i, y), srcp2, width * sizeof(T));

                srcp1 += stride1;
                srcp2 += stride2;
            }
        }

        ref.SetFromImage(std::move(jxl::ConvertToFloat(tmp1)), (linput) ? jxl::ColorEncoding::LinearSRGB(false) : jxl::ColorEncoding::SRGB(false));
        dist.SetFromImage(std::move(jxl::ConvertToFloat(tmp2)), (linput) ? jxl::ColorEncoding::LinearSRGB(false) : jxl::ColorEncoding::SRGB(false));
    }
    else
    {
        jxl::Image3F tmp1(width, height);
        jxl::Image3F tmp2(width, height);

        for (int i = 0; i < 3; ++i)
        {
            const float* srcp1 = reinterpret_cast<const float*>(src1->GetReadPtr(planes[i]));
            const float* srcp2 = reinterpret_cast<const float*>(src2->GetReadPtr(planes[i]));

            for (int y = 0; y < height; ++y)
            {
                memcpy(tmp1.PlaneRow(i, y), srcp1, width * sizeof(T));
                memcpy(tmp2.PlaneRow(i, y), srcp2, width * sizeof(T));

                srcp1 += stride1;
                srcp2 += stride2;
            }
        }

        ref.SetFromImage(std::move(tmp1), (linput) ? jxl::ColorEncoding::LinearSRGB(false) : jxl::ColorEncoding::SRGB(false));
        dist.SetFromImage(std::move(tmp2), (linput) ? jxl::ColorEncoding::LinearSRGB(false) : jxl::ColorEncoding::SRGB(false));
    }
}

class butteraugli_ : public GenericVideoFilter
{
    PClip clip;
    jxl::CodecInOut ref;
    jxl::CodecInOut dist;
    jxl::ButteraugliParams ba_params;

    template <bool distmap>
    PVideoFrame result(PVideoFrame& dst, IScriptEnvironment* env);

    void (*fill)(jxl::CodecInOut& ref, jxl::CodecInOut& dist, PVideoFrame& src1, PVideoFrame& src2);
    void (*hmap)(PVideoFrame& dst, const jxl::ImageF& distmap);
    PVideoFrame(butteraugli_::* dest)(PVideoFrame& dst, IScriptEnvironment* env);

public:
    butteraugli_(PClip _child, PClip _clip, bool distmap, float intensity_target, bool linput, IScriptEnvironment* env);
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;
    int __stdcall SetCacheHints(int cachehints, int frame_range) override
    {
        return cachehints == CACHE_GET_MTMODE ? MT_MULTI_INSTANCE : 0;
    }
};

template <bool distmap>
PVideoFrame butteraugli_::result(PVideoFrame& src2, IScriptEnvironment* env)
{
    jxl::ImageF diff_map;

    if constexpr (distmap)
    {
        PVideoFrame dst1 = env->NewVideoFrameP(vi, &src2);
        double diff_value = jxl::ButteraugliDistance(ref.Main(), dist.Main(), ba_params, jxl::GetJxlCms(), & diff_map, nullptr);
        hmap(dst1, diff_map);
        env->propSetFloat(env->getFramePropsRW(dst1), "_FrameButteraugli", diff_value, 0);

        return dst1;
    }
    else
    {
        env->MakeWritable(&src2);
        env->propSetFloat(env->getFramePropsRW(src2), "_FrameButteraugli", jxl::ButteraugliDistance(ref.Main(), dist.Main(), ba_params, jxl::GetJxlCms(), &diff_map, nullptr), 0);

        return src2;
    }
}

butteraugli_::butteraugli_(PClip _child, PClip _clip, bool distmap, float intensity_target, bool linput, IScriptEnvironment* env)
    : GenericVideoFilter(_child), clip(_clip)
{
    if (!vi.IsPlanarRGB() && !vi.IsPlanarRGBA())
        env->ThrowError("Butteraugli: the clip must be in RGB planar format.");
    
    const int bits = vi.BitsPerComponent();

    if (bits != 8 && bits != 16 && bits != 32)
        env->ThrowError("Butteraugli: the clip must be have bit depth of 8/16/32.");

    const VideoInfo vi1 = clip->GetVideoInfo();

    if (vi.height != vi1.height || vi.width != vi1.width)
        env->ThrowError("Butteraugli: the clips must have same dimensions.");
    if (!vi.IsSameColorspace(vi1))
        env->ThrowError("Butteraugli: the clips must be of the same color space.");
    if (intensity_target <= 0.0f)
        env->ThrowError("Butteraugli: intensity_target must be greater than 0.0");

    switch (vi.ComponentSize())
    {
        case 1:
            fill = (linput) ? fill_image<uint8_t, true> : fill_image<uint8_t, false>;
            hmap = heatmap<uint8_t>;
            break;
        case 2:
            fill = (linput) ? fill_image<uint16_t, true> : fill_image<uint16_t, false>;
            hmap = heatmap<uint16_t>;
            break;
        default:
            fill = (linput) ? fill_image<float, true> : fill_image<float, false>;
            hmap = heatmap<float>;
            break;
    }

    dest = (distmap) ? &butteraugli_::result<true> : &butteraugli_::result<false>;


    ref.SetSize(vi.width, vi.height);
    dist.SetSize(vi.width, vi.height);

    if (linput)
    {
        ref.metadata.m.color_encoding = jxl::ColorEncoding::LinearSRGB(false);
        dist.metadata.m.color_encoding = jxl::ColorEncoding::LinearSRGB(false);
    }
    else
    {
        ref.metadata.m.color_encoding = jxl::ColorEncoding::SRGB(false);
        dist.metadata.m.color_encoding = jxl::ColorEncoding::SRGB(false);
    }

    ba_params.hf_asymmetry = 0.8f;
    ba_params.xmul = 1.0f;
    ba_params.intensity_target = intensity_target;
}    

PVideoFrame __stdcall butteraugli_::GetFrame(int n, IScriptEnvironment* env)
{
    PVideoFrame src1 = child->GetFrame(n, env);
    PVideoFrame src2 = clip->GetFrame(n, env);

    fill(ref, dist, src1, src2);

    return (this->*dest)(src2, env);
}

AVSValue __cdecl Create_butteraugli_(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    return new butteraugli_(
        args[0].AsClip(),
        args[1].AsClip(),
        args[2].AsBool(false),
        args[3].AsFloatf(80.0f),
        args[4].AsBool(false),
        env);
}

const AVS_Linkage* AVS_linkage;

extern "C" __declspec(dllexport)
const char* __stdcall AvisynthPluginInit3(IScriptEnvironment * env, const AVS_Linkage* const vectors)
{
    AVS_linkage = vectors;

    env->AddFunction("Butteraugli", "cc[distmap]b[intensity_target]f[linput]b", Create_butteraugli_, 0);
    return "Butteraugli";
}
