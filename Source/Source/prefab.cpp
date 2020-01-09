#include "stdafx.h"
#include "prefab.h"
#include <filesystem>

#include <fstream>
#include <cereal/types/vector.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/archives/binary.hpp>

std::map<PrefabName, Prefab> PrefabManager::prefabs_;

void PrefabManager::InitPrefabs()
{
	// add basic tree prefab to list
	Prefab tree;
	for (int i = 0; i < 5; i++)
	{
		tree.Add({ 0, i, 0 }, Block(BlockType::bOakWood, 2));

		if (i > 2)
		{
			tree.Add({ -1, i, 0 }, Block(BlockType::bOakLeaves, 1));
			tree.Add({ +1, i, 0 }, Block(BlockType::bOakLeaves, 1));
			tree.Add({ 0, i, -1 }, Block(BlockType::bOakLeaves, 1));
			tree.Add({ 0, i, +1 }, Block(BlockType::bOakLeaves, 1));
		}

		if (i == 4)
			tree.Add({ 0, i + 1, 0 }, Block(BlockType::bOakLeaves, 1));
	}
	prefabs_[PrefabName::OakTree] = tree;

	Prefab bTree;
	for (int i = 0; i < 8; i++)
	{
		if (i < 7)
			bTree.Add({ 0, i, 0 }, Block(BlockType::bOakWood, 2));
		else
			bTree.Add({ 0, i, 0 }, Block(BlockType::bOakLeaves, 2));

		if (i > 4)
		{
			bTree.Add({ -1, i, 0 }, Block(BlockType::bOakLeaves, 1));
			bTree.Add({ +1, i, 0 }, Block(BlockType::bOakLeaves, 1));
			bTree.Add({ 0 , i, -1 }, Block(BlockType::bOakLeaves, 1));
			bTree.Add({ 0 , i, +1 }, Block(BlockType::bOakLeaves, 1));

			bTree.Add({ -1, i, -1 }, Block(BlockType::bOakLeaves, 1));
			bTree.Add({ +1, i, +1 }, Block(BlockType::bOakLeaves, 1));
			bTree.Add({ +1, i, -1 }, Block(BlockType::bOakLeaves, 1));
			bTree.Add({ -1, i, +1 }, Block(BlockType::bOakLeaves, 1));
		}
	}
	prefabs_[PrefabName::OakTreeBig] = bTree;

	// error prefab to be generated when an error occurs
	Prefab error;
	for (int x = 0; x < 3; x++)
	{
		for (int y = 0; y < 3; y++)
		{
			for (int z = 0; z < 3; z++)
			{
				error.Add({ x, y, z }, Block(BlockType::bError, 0xf));
			}
		}
	}
	prefabs_[PrefabName::Error] = error;

	LoadAllPrefabs();
}

// TODO: add support for xml and json archives (just check the file extension)
Prefab PrefabManager::LoadPrefabFromFile(std::string name)
{
	try
	{
		std::ifstream is(("./resources/Prefabs/" + std::string(name) + ".bin").c_str(), std::ios::binary);
		if (is.is_open())
		{
			cereal::BinaryInputArchive archive(is);
			Prefab pfb;
			archive(pfb);
			return pfb;
		}
	}
	catch (...)
	{
		//return prefabs_[Prefab::Error];
	}
	return prefabs_[PrefabName::Error];
}

void PrefabManager::LoadAllPrefabs()
{
	prefabs_[PrefabName::DungeonSmall] = LoadPrefabFromFile("dungeon");
	prefabs_[PrefabName::BorealTree] = LoadPrefabFromFile("borealTree");
	prefabs_[PrefabName::Cactus] = LoadPrefabFromFile("cactus");
	prefabs_[PrefabName::BoulderA] = LoadPrefabFromFile("boulderA");
	prefabs_[PrefabName::BoulderB] = LoadPrefabFromFile("boulderB");
	prefabs_[PrefabName::BoulderC] = LoadPrefabFromFile("boulderC");
}
