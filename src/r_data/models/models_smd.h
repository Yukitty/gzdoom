//
//---------------------------------------------------------------------------
//
// Copyright(C) 2018 John J. Muniz
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//

#ifndef __GL_MODELS_SMD_H__
#define __GL_MODELS_SMD_H__

#include "models.h"
#include <map>

class FSMDModel : public FModel
{
private:
	struct NodeName
	{
		int id, parent;
		FString name;
	};

	struct Node
	{
		FString name, parent;
		FVector3 pos, rot;
	};

	struct Vertex
	{
		Node *node;
		FVector3 pos;
		FVector3 norm;
		FVector2 tex;
	};

	struct Triangle
	{
		FTextureID mat = FNullTextureID();
		Vertex verts[3];
	};

	TArray<Node> nodelist;
	TArray<Triangle> tris;

	Node *GetNodeById(TArray<NodeName> nodeindex, int id);
	template<typename T, size_t L> T ParseVector(FScanner &sc);

public:
	FSMDModel();
	~FSMDModel();
	bool Load(const char* fn, int lumpnum, const char* buffer, int length) override;
	int FindFrame(const char* name) override;
	void RenderFrame(FModelRenderer* renderer, FTexture* skin, int frame, int frame2, double inter, int translation=0) override;
	void BuildVertexBuffer(FModelRenderer* renderer) override;
	void AddSkins(uint8_t* hitlist) override;
};

#endif
