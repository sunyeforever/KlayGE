// OGLRenderEffect.cpp
// KlayGE OpenGL渲染效果类 实现文件
// Ver 2.8.0
// 版权所有(C) 龚敏敏, 2004-2005
// Homepage: http://klayge.sourceforge.net
//
// 2.8.0
// 使用Cg实现 (2005.7.30)
//
// 2.0.4
// 初次建立 (2004.4.4)
//
// 修改记录
//////////////////////////////////////////////////////////////////////////////////

#include <KlayGE/KlayGE.hpp>
#include <KlayGE/ThrowErr.hpp>
#include <KlayGE/RenderEngine.hpp>
#include <KlayGE/RenderFactory.hpp>
#include <KlayGE/Context.hpp>
#include <KlayGE/Sampler.hpp>
#include <KlayGE/Util.hpp>

#include <sstream>
#include <iostream>
#include <boost/assert.hpp>

#include <KlayGE/OpenGL/OGLTexture.hpp>
#include <KlayGE/OpenGL/OGLMapping.hpp>
#include <KlayGE/OpenGL/OGLRenderFactory.hpp>
#include <KlayGE/OpenGL/OGLRenderEffect.hpp>

namespace KlayGE
{
	RenderTechniquePtr OGLRenderEffect::MakeRenderTechnique()
	{
		return RenderTechniquePtr(new OGLRenderTechnique(*this));
	}

	RenderPassPtr OGLRenderTechnique::MakeRenderPass()
	{
		return RenderPassPtr(new OGLRenderPass(effect_));
	}

	void OGLRenderTechnique::DoBegin(uint32_t /*flags*/)
	{
	}

	void OGLRenderTechnique::DoEnd()
	{
	}


	OGLRenderPass::~OGLRenderPass()
	{
		cgDestroyProgram(shaders_[0]);
		cgDestroyProgram(shaders_[1]);
	}

	void OGLRenderPass::DoRead()
	{
		is_validate_ = true;

		std::string vs_profile, vs_name, vs_text;
		this->shader(vs_profile, vs_name, vs_text, "vertex_shader");

		std::string ps_profile, ps_name, ps_text;
		this->shader(ps_profile, ps_name, ps_text, "pixel_shader");

		if (!vs_name.empty())
		{
			if ("auto" == vs_profile)
			{
				profiles_[0] = cgGLGetLatestProfile(CG_GL_VERTEX);
			}
			else
			{
				profiles_[0] = cgGetProfile(vs_profile.c_str());
			}

			this->create_vertex_shader(profiles_[0], vs_name, vs_text);
		}

		if (!ps_name.empty())
		{
			if ("auto" == ps_profile)
			{
				profiles_[1] = cgGLGetLatestProfile(CG_GL_FRAGMENT);
			}
			else
			{
				profiles_[1] = cgGetProfile(vs_profile.c_str());
			}

			this->create_pixel_shader(profiles_[1], ps_name, ps_text);
		}

		for (int i = 0; i < 2; ++ i)
		{
			CGparameter cg_param = cgGetFirstParameter(shaders_[i], CG_GLOBAL);
			while (cg_param)
			{
				if (cgIsParameterUsed(cg_param, shaders_[i])
					&& (CG_PARAMETERCLASS_OBJECT != cgGetParameterClass(cg_param)))
				{
					OGLRenderParameterDesc p_desc;
					p_desc.param = effect_.ParameterByName(cgGetParameterName(cg_param));
					if (p_desc.param != RenderEffectParameter::NullObject())
					{
						p_desc.cg_handle = cg_param;
						param_descs_[i].push_back(p_desc);
					}
				}

				cg_param = cgGetNextParameter(cg_param);
			}
		}
	}

	CGprogram OGLRenderPass::compile_shader(CGprofile profile, std::string const & name, std::string const & text)
	{
		OGLRenderFactory& render_factory(*checked_cast<OGLRenderFactory*>(&Context::Instance().RenderFactoryInstance()));

		CGprogram shader = cgCreateProgram(render_factory.CGContext(),
				CG_SOURCE, text.c_str(), profile, name.c_str(), NULL);

		CGerror error;
		char const * err_string = cgGetLastErrorString(&error);
		if (error != CG_NO_ERROR)
		{
#ifdef KLAYGE_DEBUG
			std::cerr << text << std::endl;
			std::cerr << err_string << std::endl;
			if (CG_COMPILER_ERROR == error)
			{
				std::cerr << cgGetLastListing(render_factory.CGContext()) << std::endl;
			}
#endif
		}

		return shader;
	}

	void OGLRenderPass::create_vertex_shader(CGprofile profile, std::string const & name, std::string const & text)
	{
		shaders_[0] = this->compile_shader(profile, name, text);
		cgGLLoadProgram(shaders_[0]);
	}

	void OGLRenderPass::create_pixel_shader(CGprofile profile, std::string const & name, std::string const & text)
	{
		shaders_[1] = this->compile_shader(profile, name, text);
		cgGLLoadProgram(shaders_[1]);
	}

	void OGLRenderPass::shader(std::string& profile, std::string& name, std::string& func, std::string const & type) const
	{
		profile.resize(0);
		name.resize(0);
		func.resize(0);

		RenderEngine::RenderStateType const state_code = render_states_define::instance().state_code(type);

		for (uint32_t i = 0; i < this->NumStates(); ++ i)
		{
			if (this->State(i).State() == state_code)
			{
				RenderEffectState const & state = this->State(i);
				shader_desc shader;
				state.Var()->Value(shader);
				profile = shader.profile;
				name = shader.func_name;
				break;
			}
		}

		std::stringstream ss;
		if (!name.empty())
		{
			for (uint32_t i = 0; i < effect_.NumParameters(); ++ i)
			{
				RenderEffectParameter& param = *effect_.ParameterByIndex(i);

				ss << type_define::instance().type_name(param.type()) << " " << param.Name();
				if (param.ArraySize() != 0)
				{
					ss << "[" << param.ArraySize() << "]";
				}

				ss << ";" << std::endl;
			}

			for (uint32_t i = 0; i < effect_.NumShaders(); ++ i)
			{
				ss << effect_.ShaderByIndex(i).str() << std::endl;
			}

			func = ss.str();
		}
	}

	void OGLRenderPass::DoBegin()
	{
		OGLRenderEngine& render_eng(*checked_cast<OGLRenderEngine*>(&Context::Instance().RenderFactoryInstance().RenderEngineInstance()));

		cgGLBindProgram(shaders_[0]);
		cgGLEnableProfile(profiles_[0]);
		cgGLBindProgram(shaders_[1]);
		cgGLEnableProfile(profiles_[1]);

		for (int i = 0; i < 2; ++ i)
		{
			for (size_t c = 0; c < param_descs_[i].size(); ++ c)
			{
				OGLRenderParameterDesc const & desc = param_descs_[i][c];

				RenderEffectParameterPtr param = desc.param;
				if (param->IsDirty())
				{
					switch (param->type())
					{
					case type_define::TC_bool:
						if (param->ArraySize() != 0)
						{
							std::vector<bool> b;
							param->Value(b);
							std::vector<int> tmp(b.begin(), b.end());
							cgSetParameterValueir(desc.cg_handle, static_cast<int>(tmp.size()), &tmp[0]);
						}
						else
						{
							bool tmp;
							param->Value(tmp);
							cgSetParameter1i(desc.cg_handle, tmp);
						}
						break;

					case type_define::TC_int:
						if (param->ArraySize() != 0)
						{
							std::vector<int> tmp;
							param->Value(tmp);
							cgSetParameterValueir(desc.cg_handle, static_cast<int>(tmp.size()), &tmp[0]);
						}
						else
						{
							int tmp;
							param->Value(tmp);
							cgSetParameter1i(desc.cg_handle, tmp);
						}
						break;

					case type_define::TC_float:
						if (param->ArraySize() != 0)
						{
							std::vector<float> tmp;
							param->Value(tmp);
							cgGLSetParameterArray1f(desc.cg_handle, 0, static_cast<long>(tmp.size()), &tmp[0]);
						}
						else
						{
							float tmp;
							param->Value(tmp);
							cgGLSetParameter1f(desc.cg_handle, tmp);
						}
						break;

					case type_define::TC_float2:
						{
							float2 tmp;
							param->Value(tmp);
							cgGLSetParameter2fv(desc.cg_handle, &tmp[0]);
						}
						break;

					case type_define::TC_float3:
						{
							float3 tmp;
							param->Value(tmp);
							cgGLSetParameter3fv(desc.cg_handle, &tmp[0]);
						}
						break;

					case type_define::TC_float4:
						if (param->ArraySize() != 0)
						{
							std::vector<float4> tmp;
							param->Value(tmp);
							cgGLSetParameterArray4f(desc.cg_handle, 0, static_cast<long>(tmp.size()), &tmp[0][0]);
						}
						else
						{
							float4 tmp;
							param->Value(tmp);
							cgGLSetParameter4fv(desc.cg_handle, &tmp[0]);
						}
						break;

					case type_define::TC_float4x4:
						if (param->ArraySize() != 0)
						{
							std::vector<float4x4> tmp;
							param->Value(tmp);
							cgGLSetMatrixParameterArrayfr(desc.cg_handle, 0, static_cast<long>(tmp.size()), &tmp[0][0]);
						}
						else
						{
							float4x4 tmp;
							param->Value(tmp);
							cgGLSetMatrixParameterfr(desc.cg_handle, &tmp[0]);
						}
						break;
					}
				}
			}
		}

		for (int i = 0; i < 2; ++ i)
		{
			for (size_t c = 0; c < param_descs_[i].size(); ++ c)
			{
				OGLRenderParameterDesc const & desc = param_descs_[i][c];
				if (desc.param->type() == type_define::TC_sampler)
				{
					SamplerPtr s;
					desc.param->Value(s);
					render_eng.SetSampler(cgGLGetTextureEnum(desc.cg_handle) - GL_TEXTURE0, s);
				}
			}
		}
	}

	void OGLRenderPass::DoEnd()
	{
		OGLRenderEngine& render_eng(*checked_cast<OGLRenderEngine*>(&Context::Instance().RenderFactoryInstance().RenderEngineInstance()));

		cgGLDisableProfile(profiles_[0]);
		cgGLDisableProfile(profiles_[1]);

		for (int i = 0; i < 2; ++ i)
		{
			for (size_t c = 0; c < param_descs_[i].size(); ++ c)
			{
				OGLRenderParameterDesc const & desc = param_descs_[i][c];
				if (desc.param->type() == type_define::TC_sampler)
				{
					render_eng.DisableSampler(cgGLGetTextureEnum(desc.cg_handle) - GL_TEXTURE0);
				}
			}
		}
	}
}
