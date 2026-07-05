// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/rasterizer_interface.h"
#include "video_core/shader/generator/pica_fs_config.h"
#include "video_core/shader/generator/shader_uniforms.h"

namespace Memory {
class MemorySystem;
}

namespace Pica {
class PicaCore;
}

namespace VideoCore {

class RasterizerAccelerated : public RasterizerInterface {
public:
    explicit RasterizerAccelerated(Memory::MemorySystem& memory, Pica::PicaCore& pica);
    virtual ~RasterizerAccelerated() = default;

    void AddTriangle(const Pica::OutputVertex& v0, const Pica::OutputVertex& v1,
                     const Pica::OutputVertex& v2) override;

protected:
    /// Sync vertex and framgent uniforms from PICA registers
    void SyncDrawUniforms();

    void SetVRData(const int32_t  &vrImmersiveMode, const float& immersiveModeFactor, int uoffset, const float& gamePosScaler, const float inv_view[16]) override;

protected:
    /// Structure that the hardware rendered vertices are composed of
    struct HardwareVertex {
        HardwareVertex() = default;
        HardwareVertex(const Pica::OutputVertex& v, bool flip_quaternion);

        Common::Vec4f position;
        Common::Vec4f color;
        Common::Vec2f tex_coord0;
        Common::Vec2f tex_coord1;
        Common::Vec2f tex_coord2;
        float tex_coord0_w;
        Common::Vec4f normquat;
        Common::Vec3f view;
    };

    struct VertexArrayInfo {
        u32 vs_input_index_min;
        u32 vs_input_index_max;
        u32 vs_input_size;
    };

    /// Retrieve the range and the size of the input vertex
    VertexArrayInfo AnalyzeVertexArray(bool is_indexed, u32 stride_alignment = 1);

protected:
    Memory::MemorySystem& memory;
    Pica::PicaCore& pica;
    Pica::RegsInternal& regs;
    std::vector<HardwareVertex> vertex_batch;
    Pica::Shader::UserConfig user_config{};
    Pica::Shader::Generator::VSUniformData vs_data{};
    Pica::Shader::Generator::FSUniformData fs_data{};
    bool vs_data_dirty = true;
    bool fs_data_dirty = true;

    // VR data injected into PICA vertex shader uniforms for immersive modes.
    u32 vr_uoffset = 0;
    u32 vr_immersive_mode = 0;
    float vr_game_pos_scaler = 0.f;
    float vr_inv_view[16] = {};

    struct HeuristicResult {
        int32_t view_matrixregister = -1;
        int32_t eye_indicator_register = -1;
        int32_t eye_indicator_reg_index = -1;
    } vr_heuristic;

public:
    void ApplyVRDataToPicaVSUniforms(Pica::Shader::Generator::VSPicaUniformData& vs_uniforms);
};

} // namespace VideoCore
