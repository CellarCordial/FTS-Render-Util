#ifndef RENDER_PASS_AERIAL_LUT_H
#define RENDER_PASS_AERIAL_LUT_H

#include "../../render_graph/render_graph.h"
#include "../../core/math/matrix.h"
#include <memory>


namespace fantasy
{
	namespace constant
	{
		struct AerialLUTPassConstant
		{
			float3 sun_direction;		float sun_theta = 0.0f;
			float3 frustum_a;			float max_aerial_distance = 2000.0f;
			float3 frustum_b;			int32_t per_slice_march_step_count = 1;
			float3 frustum_c;			float camera_height = 0.0f;
			float3 frustum_d;			uint32_t enable_multi_scattering = true;
			float3 camera_position;	uint32_t enable_shadow = true;
			float world_scale = 0.0f;	float3 pad;
			float4x4 shadow_view_proj;
		};
	}


	class FAerialLUTPass : public RenderPassInterface
	{
	public:
		FAerialLUTPass() { type = RenderPassType::Compute; }

		bool compile(DeviceInterface* device, RenderResourceCache* cache) override;
		bool execute(CommandListInterface* cmdlist, RenderResourceCache* cache) override;

		friend class AtmosphereTest;

	private:
		bool _resource_writed = false;
		constant::AerialLUTPassConstant _pass_constant;

		std::shared_ptr<BufferInterface> _pass_constant_buffer;
		std::shared_ptr<TextureInterface> _aerial_lut_texture;

		std::unique_ptr<BindingLayoutInterface> _binding_layout;

		std::unique_ptr<Shader> _cs;
		std::unique_ptr<ComputePipelineInterface> _pipeline;
		
		std::unique_ptr<BindingSetInterface> _binding_set;
		ComputeState _compute_state;
	};


}



















#endif