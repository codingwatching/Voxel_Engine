#pragma once
#include <functional>

class VAO;
class IBO;
class Shader;
class DirLight;
class Sun;
typedef struct Chunk* ChunkPtr;
typedef std::function<void()> DrawCB;
typedef std::function<void(const glm::mat4&)> ModelCB;

// responsible for making stuff appear on the screen
class Renderer
{
public:
	Renderer();
	void Init();

	// interation
	void DrawAll();
	void Clear();

	void SetDirLight(DirLight* d) { activeDirLight_ = d; }
	void SetSun(Sun* s) { activeSun_ = s; }

private:
	// broad-phase rendering
	void drawShadows(); // construct shadow map(s)
	void drawSky();
	void drawNormal();	// draw what we see
	void drawWater();
	void drawPostProcessing(); // apply post processing effects

	// narrow-phase rendering
	void drawChunks(bool cullFrustum, 
		DrawCB predraw_cb, 
		ModelCB draw_cb, 
		DrawCB postdraw_cb);
	void drawBillboard(VAO* vao, size_t count, DrawCB uniform_cb);
	void drawQuad();

	// debug
	void drawDepthMapsDebug();
	void drawAxisIndicators();

	// deferred rendering
	void initDeferredBuffers();
	void geometryPass();
	void lightingPass();

	// post processing
	void initPPBuffers();
	void postProcess();

	DirLight* activeDirLight_;
	Sun* activeSun_;

	// ssr & deferred rendering
	unsigned gBuffer; // framebuffer
	unsigned gPosition, gNormal, gAlbedoSpec, gDepth;
	unsigned rboDepth; // depth renderbuffer

	// pp
	unsigned pBuffer;
	unsigned pColor;
	unsigned pDepth;
};