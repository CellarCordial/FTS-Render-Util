#include "sun_disk.h"
#include "../../shader/shader_compiler.h"
#include "../../core/tools/check_cast.h"
#include "../../scene/camera.h"
#include "../../scene/light.h"
#include <memory>


namespace fantasy
{
#define SUN_DISK_SEGMENT_NUM 32

	void SunDiskPass::GenerateSunDiskVertices()
	{
		for (uint32_t ix = 0; ix < SUN_DISK_SEGMENT_NUM; ++ix)
		{
			const float cfPhiBegin = lerp(0.0f, 2.0f * PI, static_cast<float>(ix) / SUN_DISK_SEGMENT_NUM);
			const float cfPhiEnd = lerp(0.0f, 2.0f * PI, static_cast<float>(ix + 1) / SUN_DISK_SEGMENT_NUM);

			const Vector2F A = {};
			const Vector2F B = { std::cos(cfPhiBegin), std::sin(cfPhiBegin) };
			const Vector2F C = { std::cos(cfPhiEnd), std::sin(cfPhiEnd) };

			_sun_disk_vertices.push_back(Vertex{ .position = A });
			_sun_disk_vertices.push_back(Vertex{ .position = B });
			_sun_disk_vertices.push_back(Vertex{ .position = C });
		}
	}


	bool SunDiskPass::compile(DeviceInterface* device, RenderResourceCache* cache)
	{	
		// Binding Layout.
		{
			BindingLayoutItemArray binding_layout_items(4);
			binding_layout_items[0] = BindingLayoutItem::create_constant_buffer(0, false);
			binding_layout_items[1] = BindingLayoutItem::create_push_constants(1, sizeof(constant::SunDiskPassConstant));
			binding_layout_items[2] = BindingLayoutItem::create_texture_srv(0);
			binding_layout_items[3] = BindingLayoutItem::create_sampler(0);
			ReturnIfFalse(_binding_layout = std::unique_ptr<BindingLayoutInterface>(device->create_binding_layout(
				BindingLayoutDesc{ .binding_layout_items = binding_layout_items }
			)));
		}

		// Input Layout.
		{
			VertexAttributeDescArray vertex_attribute_desc(1);
			vertex_attribute_desc[0].name = "POSITION";
			vertex_attribute_desc[0].format = Format::RG32_FLOAT;
			vertex_attribute_desc[0].element_stride = sizeof(Vertex);
			vertex_attribute_desc[0].offset = offsetof(Vertex, position);
			ReturnIfFalse(_input_layout = std::unique_ptr<InputLayoutInterface>(device->create_input_layout(
				vertex_attribute_desc.data(),
				vertex_attribute_desc.size(),
				nullptr
			)));
		}

		// Shader.
		{
			ShaderCompileDesc shader_compile_desc;
			shader_compile_desc.shader_name = "atmosphere/sun_disk.hlsl";
			shader_compile_desc.entry_point = "vertex_shader";
			shader_compile_desc.target = ShaderTarget::Vertex;
			ShaderData vs_data = shader_compile::compile_shader(shader_compile_desc);
			shader_compile_desc.entry_point = "pixel_shader";
			shader_compile_desc.target = ShaderTarget::Pixel;
			ShaderData ps_data = shader_compile::compile_shader(shader_compile_desc);

			ShaderDesc vs_desc;
			vs_desc.entry = "vertex_shader";
			vs_desc.shader_type = ShaderType::Vertex;
			ReturnIfFalse(_vs = std::unique_ptr<Shader>(create_shader(vs_desc, vs_data.data(), vs_data.size())));

			ShaderDesc ps_desc;
			ps_desc.shader_type = ShaderType::Pixel;
			ps_desc.entry = "pixel_shader";
			ReturnIfFalse(_ps = std::unique_ptr<Shader>(create_shader(ps_desc, ps_data.data(), ps_data.size())));
		}

		// Buffer.
		{
			GenerateSunDiskVertices();
			ReturnIfFalse(_vertex_buffer = std::shared_ptr<BufferInterface>(device->create_buffer(
				BufferDesc::create_vertex(
					sizeof(Vertex) * SUN_DISK_SEGMENT_NUM * 3, 
					"SunDiskVertexBuffer"
				)
			)));
		}

		// Frame Buffer.
		{
			FrameBufferDesc frame_buffer_desc;
			frame_buffer_desc.color_attachments.push_back(
				FrameBufferAttachment::create_attachment(
					check_cast<TextureInterface>(cache->require("FinalTexture"))
				)
			);
			frame_buffer_desc.depth_stencil_attachment = 
				FrameBufferAttachment::create_attachment(check_cast<TextureInterface>(cache->require("DepthTexture")));
			ReturnIfFalse(_frame_buffer = std::unique_ptr<FrameBufferInterface>(device->create_frame_buffer(frame_buffer_desc)));
		}

		// Pipeline.
		{
			GraphicsPipelineDesc pipeline_desc;
			pipeline_desc.render_state.depth_stencil_state.enable_depth_test = true;
			pipeline_desc.render_state.depth_stencil_state.enable_depth_write = false;
			pipeline_desc.render_state.depth_stencil_state.depth_func = ComparisonFunc::LessOrEqual;
			pipeline_desc.vertex_shader = _vs.get();
			pipeline_desc.pixel_shader = _ps.get();
			pipeline_desc.binding_layouts.push_back(_binding_layout.get());
			pipeline_desc.input_layout = _input_layout.get();
			ReturnIfFalse(_pipeline = std::unique_ptr<GraphicsPipelineInterface>(
				device->create_graphics_pipeline(pipeline_desc, _frame_buffer.get())
			));
		}

		// Binding Set.
		{
			BindingSetItemArray binding_set_items(4);
			binding_set_items[0] = BindingSetItem::create_constant_buffer(0, check_cast<BufferInterface>(cache->require("AtmospherePropertiesBuffer")));
			binding_set_items[1] = BindingSetItem::create_push_constants(1, sizeof(constant::SunDiskPassConstant));
			binding_set_items[2] = BindingSetItem::create_texture_srv(0, check_cast<TextureInterface>(cache->require("TransmittanceTexture")));
			binding_set_items[3] = BindingSetItem::create_sampler(0, check_cast<SamplerInterface>(cache->require("LinearClampSampler")));
			ReturnIfFalse(_binding_set = std::unique_ptr<BindingSetInterface>(device->create_binding_set(
				BindingSetDesc{ .binding_items = binding_set_items },
				_binding_layout.get()
			)));
		}

		// Graphics state.
		{
			_graphics_state.pipeline = _pipeline.get();
			_graphics_state.frame_buffer = _frame_buffer.get();
			_graphics_state.binding_sets.push_back(_binding_set.get());
			_graphics_state.vertex_buffer_bindings.push_back(VertexBufferBinding{ .buffer = _vertex_buffer });
			_graphics_state.viewport_state = ViewportState::create_default_viewport(CLIENT_WIDTH, CLIENT_HEIGHT);
		}

		return true;
	}

	bool SunDiskPass::execute(CommandListInterface* cmdlist, RenderResourceCache* cache)
	{
		ReturnIfFalse(cmdlist->open());

		if (!_resource_writed)
		{
			ReturnIfFalse(cmdlist->write_buffer(_vertex_buffer.get(), _sun_disk_vertices.data(), _sun_disk_vertices.size() * sizeof(Vertex)));
			_resource_writed = true;
		}

		{
			float* world_scale;
			ReturnIfFalse(cache->require_constants("WorldScale", reinterpret_cast<void**>(&world_scale)));

			Vector3F LightDirection;
			ReturnIfFalse(cache->get_world()->each<DirectionalLight>(
				[this, &LightDirection](Entity* entity, DirectionalLight* light) -> bool
				{
					_pass_constant.sun_theta = std::asin(-light->direction.y);
					_pass_constant.sun_radius = Vector3F(light->intensity * light->color);

					LightDirection = light->direction;
					return true;
				}
			));

			ReturnIfFalse(cache->get_world()->each<Camera>(
				[&](Entity* entity, Camera* camera) -> bool
				{
					_pass_constant.camera_height = camera->position.y * (*world_scale);

					Matrix3x3 OrthogonalBasis = create_orthogonal_basis_from_z(LightDirection);
					Matrix4x4 world_matrix = mul(
						scale(Vector3F(_sun_disk_size)),
						mul(
							Matrix4x4(OrthogonalBasis),
							mul(translate(camera->position), translate(-LightDirection))
						)
					);
					_pass_constant.world_view_proj = mul(world_matrix, camera->get_view_proj());

					return true;
				}
			));
		}

		ReturnIfFalse(cmdlist->set_graphics_state(_graphics_state));
		ReturnIfFalse(cmdlist->set_push_constants(&_pass_constant, sizeof(constant::SunDiskPassConstant)));
		ReturnIfFalse(cmdlist->draw(DrawArguments{ .index_count = SUN_DISK_SEGMENT_NUM * 3 }));

		ReturnIfFalse(cmdlist->close());

		return true;
	}

	bool SunDiskPass::finish_pass()
	{
		if (!_sun_disk_vertices.empty())
		{
			 _sun_disk_vertices.clear();
			 _sun_disk_vertices.shrink_to_fit();
		}
		return true;
	}

}