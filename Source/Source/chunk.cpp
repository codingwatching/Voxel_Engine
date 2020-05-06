#include "stdafx.h"

#include <Pipeline.h>
#include "Renderer.h"
#include <camera.h>
#include <Frustum.h>

#include "vbo.h"
#include "vao.h"
#include "ibo.h"
#include "chunk.h"
#include "block.h"
#include "shader.h"
#include <Vertices.h>
#include <sstream>
#include "settings.h"
#include "misc_utils.h"
#include "ChunkStorage.h"


Chunk::Chunk() : storage(CHUNK_SIZE_CUBED)
{
}


Chunk::~Chunk()
{
}


Chunk::Chunk(const Chunk& other) : storage(CHUNK_SIZE_CUBED)
{
	*this = other;
}


// copy assignment operator for serialization
Chunk& Chunk::operator=(const Chunk& rhs)
{
	//this->pos_ = rhs.pos_;
	this->SetPos(rhs.pos_);
	this->storage = rhs.storage;
	return *this;
}


void Chunk::Update()
{

	// in the future, make this function perform other tick update actions,
	// such as updating N random blocks (like in Minecraft)
}


void Chunk::Render()
{
	//ASSERT(vao_ && positions_ && normals_ && colors_);
	if (vao_)
	{
		vao_->Bind();
		//glDrawArrays(GL_TRIANGLES, 0, vertexCount_);
		ibo_->Bind();
		glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, (void*)0);
	}
}


void Chunk::RenderWater()
{
	//if (active_ && wvao_)
	//{
	//	wvao_->Bind();
	//	//glDrawArrays(GL_TRIANGLES, 0, wvertexCount_);
	//	wibo_->Bind();
	//	glDrawElements(GL_TRIANGLES, windexCount_, GL_UNSIGNED_INT, (void*)0);
	//}
}


void Chunk::BuildBuffers()
{
	std::lock_guard<std::mutex> lock(vertex_buffer_mutex_);
	// generate various vertex buffers
	{
		if (!vao_)
			vao_ = std::make_unique<VAO>();

		ibo_ = std::make_unique<IBO>(&tIndices[0], tIndices.size());

		vao_->Bind();
		encodedStuffVbo_ = std::make_unique<VBO>(encodedStuffArr.data(), sizeof(GLfloat) * encodedStuffArr.size());
		encodedStuffVbo_->Bind();
		glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, sizeof(GLfloat), (void*)0); // encoded stuff
		glEnableVertexAttribArray(0);

		lightingVbo_ = std::make_unique<VBO>(lightingArr.data(), sizeof(GLfloat) * lightingArr.size());
		lightingVbo_->Bind();
		glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(GLfloat), (void*)0); // encoded lighting
		glEnableVertexAttribArray(1);

		vertexCount_ = encodedStuffArr.size();
		indexCount_ = tIndices.size();
		tIndices.clear();
		encodedStuffArr.clear();
		lightingArr.clear();
		//tIndices.resize(0);
		//encodedStuffArr.resize(0);
		//lightingArr.resize(0);
	}
}


#include <iomanip>
void Chunk::BuildMesh()
{
	high_resolution_clock::time_point benchmark_clock_ = high_resolution_clock::now();

	for (int i = 0; i < fCount; i++)
	{
		nearChunks[i] = ChunkStorage::GetChunk(
			this->pos_ + ChunkHelpers::faces[i]);
	}
	std::lock_guard<std::mutex> lock(vertex_buffer_mutex_);

	// absolute worst case (chunk consisting of checkered cubes)
	//encodedStuffArr.reserve(393216);
	//lightingArr.reserve(393216);

	for (int z = 0; z < CHUNK_SIZE; z++)
	{
		// precompute first flat index part
		int zcsq = z * CHUNK_SIZE_SQRED;
		for (int y = 0; y < CHUNK_SIZE; y++)
		{
			// precompute second flat index part
			int yczcsq = y * CHUNK_SIZE + zcsq;
			for (int x = 0; x < CHUNK_SIZE; x++)
			{
				// this is what we would be doing every innermost iteration
				//int index = x + y * CHUNK_SIZE + z * CHUNK_SIZE_SQRED;
				// we only need to do addition
				int index = x + yczcsq;

				// skip fully transparent blocks
				const Block block = BlockAt(index);
				if (Block::PropertiesTable[block.GetTypei()].invisible)
					continue;

				buildBlockVertices_normal({ x, y, z }, block);
			}
		}
	}

	duration<double> benchmark_duration_ = duration_cast<duration<double>>(high_resolution_clock::now() - benchmark_clock_);
	double milliseconds = benchmark_duration_.count() * 1000;
	if (accumcount > 1000)
	{
		accumcount = 0;
		accumtime = 0;
	}
	accumtime = accumtime + milliseconds;
	accumcount = accumcount + 1;
	//std::cout 
	//	<< std::setw(-2) << std::showpoint << std::setprecision(4) << accumtime / accumcount << " ms "
	//	<< "(" << milliseconds << ")"
	//	<< std::endl;
}


void Chunk::buildBlockVertices_normal(const glm::ivec3& pos, Block block)
{
	for (int f = Far; f < fCount; f++)
		buildBlockFace(f, pos, block);
}


void Chunk::buildBlockFace(
	int face,
	const glm::ivec3& blockPos,	// position of current block
	Block block)								// block-specific information)
{
	using namespace glm;
	using namespace ChunkHelpers;
	thread_local static localpos nearblock; // avoids unnecessary construction of vec3s
	//glm::ivec3 nearFace = blockPos + faces[face];

	nearblock.block_pos = blockPos + faces[face];

	ChunkPtr nearChunk = this;

	// if neighbor is out of this chunk, find which chunk it is in
	if (any(lessThan(nearblock.block_pos, ivec3(0))) || any(greaterThanEqual(nearblock.block_pos, ivec3(CHUNK_SIZE))))
	{
		fastWorldPosToLocalPos(chunkPosToWorldPos(nearblock.block_pos, pos_), nearblock);
		nearChunk = nearChunks[face];
	}

	// for now, we won't make a mesh for faces adjacent to NULL chunks
	// in the future it may be wise to construct the mesh regardless
	if (nearChunk == nullptr)
		return;

	// neighboring block and light
	Block block2 = nearChunk->BlockAt(nearblock.block_pos);
	Light light = block2.GetLight();
	//Light light = nearChunk->LightAtCheap(nearblock.block_pos);

	// this block is water and other block isn't water and is above this block
	if (block2.GetType() != BlockType::bWater && block.GetType() == BlockType::bWater && (nearblock.block_pos - blockPos).y > 0)
	{
		addQuad(blockPos, block, face, nearChunk, light);
		return;
	}
	// other block isn't air or water - don't add mesh
	if (block2.GetType() != BlockType::bAir && block2.GetType() != BlockType::bWater)
		return;
	// both blocks are water - don't add mesh
	if (block2.GetType() == BlockType::bWater && block.GetType() == BlockType::bWater)
		return;
	// this block is invisible - don't add mesh
	if (Block::PropertiesTable[int(block.GetType())].invisible)
		return;

	// if all tests are passed, generate this face of the block
	addQuad(blockPos, block, face, nearChunk, light);
}


void Chunk::addQuad(const glm::ivec3& lpos, Block block, int face, ChunkPtr nearChunk, Light light)
{
	int normalIdx = face;
	int texIdx = 100; // temp
	uint16_t lighting = light.Raw();
	light.SetS(15);

	// add 4 vertices representing a quad
	float aoValues[4] = { 0, 0, 0, 0 }; // AO for each quad
	int aoValuesIndex = 0;
	const GLfloat* data = Vertices::cube_light;
	int endQuad = (face + 1) * 12;
	for (int i = face * 12; i < endQuad; i += 3)
	{
		using namespace ChunkHelpers;
		// transform vertices relative to chunk
		glm::vec3 vert(data[i + 0], data[i + 1], data[i + 2]);
		glm::uvec3 finalVert = glm::ceil(vert) + glm::vec3(lpos);// +0.5f;
		
		int cornerIdx = 2; // temp

		// compress attributes into 32 bits
		GLuint encoded = Encode(finalVert, normalIdx, texIdx, cornerIdx);

		int invOcclusion = 6;
		if (Settings::Graphics.blockAO)
			invOcclusion = 2 * vertexFaceAO(lpos, vert, faces[face]);
		aoValues[aoValuesIndex++] = invOcclusion;
		invOcclusion = 6 - invOcclusion;
		auto tLight = light;
		tLight.Set(tLight.Get() - glm::min(tLight.Get(), glm::u8vec4(invOcclusion)));
		lighting = tLight.Raw();

		// preserve bit ordering
		encodedStuffArr.push_back(glm::uintBitsToFloat(encoded));
		lightingArr.push_back(glm::uintBitsToFloat(lighting));
	}

	// add 6 indices defining 2 triangles from that quad
	int endIndices = (face + 1) * 6;
	if (aoValues[0] + aoValues[2] > aoValues[1] + aoValues[3])
	{
		for (int i = face * 6; i < endIndices; i++)
		{
			tIndices.push_back(Vertices::cube_indices_light_cw_anisotropic[i] + encodedStuffArr.size() - 4);
		}
	}
	else
	{
		for (int i = face * 6; i < endIndices; i++)
		{
			// refer to just placed vertices (4 of them)
			tIndices.push_back(Vertices::cube_indices_light_cw[i] + encodedStuffArr.size() - 4);
		}
	}
}


// TODO: when making this function, use similar near chunk checking scheme to
// minimize amount of searching in the global chunk map
int Chunk::vertexFaceAO(const glm::vec3& lpos, const glm::vec3& cornerDir, const glm::vec3& norm)
{
	// TODO: make it work over chunk boundaries
	using namespace glm;

	int occluded = 0;

	// sides are components of the corner minus the normal direction
	vec3 sidesDir = cornerDir * 2.0f - norm;
	for (int i = 0; i < sidesDir.length(); i++)
	{
		if (sidesDir[i] != 0)
		{
			vec3 sideDir(0);
			sideDir[i] = sidesDir[i];
			vec3 sidePos = lpos + sideDir + norm;
			if (all(greaterThanEqual(sidePos, vec3(0))) && all(lessThan(sidePos, vec3(CHUNK_SIZE))))
				if (BlockAt(ivec3(sidePos)).GetType() != BlockType::bAir)
					occluded++;
		}
	}


	if (occluded == 2)
		return 0;
	
	vec3 cornerPos = lpos + (cornerDir * 2.0f);
	if (all(greaterThanEqual(cornerPos, vec3(0))) && all(lessThan(cornerPos, vec3(CHUNK_SIZE))))
		if (BlockAt(ivec3(cornerPos)).GetType() != BlockType::bAir)
			occluded++;

	return 3 - occluded;
}