#pragma once
#include <string>
#include <vector>

#include "CommandList.h"

class Mesh;

class ModelLoader
{
public:
	static std::vector<std::unique_ptr<Mesh>> LoadObj(CommandList& commandList, const std::wstring& path, bool rhCoords = false);
};
