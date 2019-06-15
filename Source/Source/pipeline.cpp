#include "stdafx.h"
#include "shader.h"
#include "vbo.h"
#include "ibo.h"
#include "vao.h"
#include "render.h"
#include "texture.h"
#include "lit_mesh.h"
#include "camera.h"
#include "utilities.h"
#include "level.h"
#include "render_data.h"
#include "pipeline.h"
#include "transform.h"
#include "block.h"
#include <thread>

#define ID2D(x, y, w) width * row + col

// graphics pipeline: the order in which various "things" will be rendered

static void GLAPIENTRY
GLerrorCB(GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const GLchar* message,
	const void* userParam)
{
	std::cout << "GL CALLBACK: "
		<< (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "")
		<< " type = 0x" << type
		<< ", severity = 0x" << severity
		<< ", message = " << message << std::endl;
	//fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
	//	(type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
	//	type, severity, message);
}

namespace Render
{
	std::unordered_map<const char*, Texture*, Utils::djb2hash, Utils::charPtrKeyEq> textures;
	Camera* currCamera;
	ShaderPtr currShader;
	Renderer renderer;

	constexpr int MAX_BLOCKS = 1000000;
	constexpr int MAX_THREADS = 6;
	glm::vec4 colorsVEC[MAX_BLOCKS];
	glm::mat4 modelsMAT[MAX_BLOCKS];
	std::vector<unsigned> updatedBlocks;
	VAO* blockVao;
	VBO* blockMeshBuffer;
	VBO* blockModelBuffer;
	VBO* blockColorBuffer;
	LevelPtr currLevel = nullptr;
	std::vector<std::thread> threads;

	void CalcModels(int low, int high)
	{
		for (int i = low; i < high; i++)
		{
			modelsMAT[updatedBlocks[i]] = currLevel->GetBlocks()[updatedBlocks[i]]->GetModel();
			colorsVEC[updatedBlocks[i]] = currLevel->GetBlocks()[updatedBlocks[i]]->clr;
		}
	}

	void Init()
	{
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(GLerrorCB, NULL);
		glEnable(GL_DEPTH_TEST);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CCW);

		currShader = Shader::shaders["flat"] = new Shader("flat_color_instanced.vs", "flat_color_instanced.fs");

		blockVao = new VAO();
		blockVao->Bind();
		blockMeshBuffer = new VBO(Render::cube_tex_vertices, sizeof(Render::cube_tex_vertices));
		//VBOlayout meshLayout;
		//meshLayout.Push<float>(3);
		//meshLayout.Push<float>(2);
		//blockVao->AddBuffer(*blockMeshBuffer, meshLayout);
		blockColorBuffer = new VBO(nullptr, MAX_BLOCKS * sizeof(glm::vec4), GL_STREAM_DRAW);
		blockModelBuffer = new VBO(nullptr, MAX_BLOCKS * sizeof(glm::mat4), GL_STREAM_DRAW);
	}

	// currently being used to draw everything
	void Draw(LevelPtr level)
	{
		currShader->Use();
		currLevel = level;

		// new draw method (mash every object's data into one vbo/vao and draw that)
		glm::mat4 view = currCamera->GetView();
		glm::mat4 proj = currCamera->GetProj();
		glm::mat4 viewProjection = proj * view;

		//VBO* vbo = new VBO(Render::cube_tex_vertices, sizeof(Render::cube_tex_vertices));
		div_t work = div(updatedBlocks.size(), MAX_THREADS);

		// construct an array of model matrices for OpenGL
		for (int i = 0; i < MAX_THREADS; i++)
		{
			int realWork = work.quot + (i < MAX_THREADS - 1 ? 0 : work.rem);
			threads.push_back(std::thread(CalcModels, i * work.quot, i * work.quot + realWork));
		}

		for (auto& thread : threads)
			thread.join();
		threads.clear();

		blockModelBuffer->Bind();
		glBufferData(GL_ARRAY_BUFFER, MAX_BLOCKS * sizeof(glm::mat4), NULL, GL_STREAM_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, 0, level->GetBlocks().size() * sizeof(glm::mat4), modelsMAT);
		
		blockMeshBuffer->Bind();
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*)0); // screenpos

		blockModelBuffer->Bind();
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(float) * 0));
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(float) * 4));
		glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(float) * 8));
		glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(float) * 12));

		blockColorBuffer->Bind();
		glBufferData(GL_ARRAY_BUFFER, MAX_BLOCKS * sizeof(glm::vec4), NULL, GL_STREAM_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, 0, level->GetBlocks().size() * sizeof(glm::vec4), colorsVEC);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		glEnableVertexAttribArray(3);
		glEnableVertexAttribArray(4);
		glEnableVertexAttribArray(5);
		glVertexAttribDivisor(0, 0);
		glVertexAttribDivisor(1, 1);
		glVertexAttribDivisor(2, 1);
		glVertexAttribDivisor(3, 1);
		glVertexAttribDivisor(4, 1);
		glVertexAttribDivisor(5, 1);

		currShader->setMat4("u_viewProj", viewProjection);

		glDrawArraysInstanced(GL_TRIANGLES, 0, 36, level->GetBlocks().size());
		//// old draw method (draw call for each object = inefficient)
		//static bool first = true;
		//for (auto& obj : level->GetObjects())
		//{
		//	if (obj->GetEnabled())
		//	{
		//		RenderDataPtr rend = obj->GetComponent<RenderData>();
		//		if (rend && rend->GetEnabled())
		//		{
		//			if (currShader != rend->GetShader() || first)
		//			{
		//				currShader = rend->GetShader();
		//				currShader->Use();
		//				currShader->setMat4("u_proj", currCamera->GetProj());
		//				first = false;
		//			}

		//			currShader->setMat4("u_view", currCamera->GetView());
		//			currShader->setVec4("u_color", rend->GetColor());
		//			currShader->setMat4("u_model", obj->GetComponent<Transform>()->GetModel());

		//			renderer.DrawArrays(rend->GetVao(), 36, *currShader);
		//			
		//		}
		//	}
		//}
		//delete models;
		updatedBlocks.clear();
	}

	void SetCamera(Camera* cam)
	{
		currCamera = cam;
	}

	void drawImGui()
	{
		ImGui::Begin("Giraffix");

		//ImGui::SliderFloat3("TranslationA", &translationA.x, 0, 1000);
		//ImGui::SliderFloat3("TranslationB", &translationB.x, 0, 1000);
		//ImGui::SliderFloat3("Camera Pos", &camerapos.x, -500, 500);

		ImGui::End();
	}

	void Terminate()
	{

	}
}