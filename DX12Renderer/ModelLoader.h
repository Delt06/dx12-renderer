#pragma once
#include <string>
#include <vector>

#include "CommandList.h"

class Mesh;

class ModelLoader
{
public:
	static std::vector<std::unique_ptr<Mesh>> LoadObj(CommandList& commandList, const std::string& path, bool rhCoords = false);
};
