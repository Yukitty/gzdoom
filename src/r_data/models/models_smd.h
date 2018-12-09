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

#ifndef __R_MODELS_SMD_H__
#define __R_MODELS_SMD_H__

#include "models.h"
#include "r_data/models/anims_smd.h"

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
		FString name;
		FVector3 pos;
		FVector4 rot;
		Node *parent;
	};

	struct RNode
	{
		FVector3 pos;
		FVector4 rot;
		FVector3 xyz;
		float s, dot;
	};

	struct Weight
	{
		FString nodeName;
		float bias;
		FVector3 pos;
	};

	struct Vertex
	{
		Node *node;
		FVector3 pos, normal;
		FVector2 texCoord;
		Weight weight[8];
	};

	struct Triangle
	{
		Vertex vertex[3];
	};

	struct Surface
	{
		FTextureID material = FNullTextureID();
		TArray<Triangle> triangle;
	};

	struct Animation
	{
		unsigned int start, frames;
		FSMDAnim data;
	};

	TMap<FName, Node> nodes;
	TArray<Surface> surfaceList;
	unsigned int vbufSize = 0;

	Node *GetNodeById(TArray<NodeName> nodeindex, int id);
	template<typename T, size_t L> T ParseVector(FScanner &sc);
	FVector3 CalcVertOff(FVector3 pos, RNode bone);

	TMap<FName, unsigned int> animNameIndex; // fast name -> animList index
	TArray<Animation> animList; // depends on strict ordering for consistent frameno.
	unsigned int frameCount = 0; // to make tacking new animations onto the end easier.

	TMap<FName, RNode> *FlattenSkeleton();

public:
	bool Load(const char* fn, int lumpnum, const char* buffer, int length) override;
	int FindFrame(const char* name) override;
	void RenderFrame(FModelRenderer* renderer, FTexture* skin, int frame, int frame2, double inter, int translation=0) override;
	void BuildVertexBuffer(FModelRenderer* renderer) override;
	void AddSkins(uint8_t* hitlist) override;

	bool CanLoadAnim() override { return true; };
	void LoadAnim(const char *path, const char *name, int lumpnum) override;

	friend class FSMDAnim;
};

#endif
