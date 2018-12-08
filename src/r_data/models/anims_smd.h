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

#ifndef __R_ANIMS_SMD_H__
#define __R_ANIMS_SMD_H__

class FSMDModel;

class FSMDAnim
{
private:
	struct Node
	{
		FVector3 pos;
		FVector4 rot;
	};

	struct FrameData
	{
		unsigned int time;
		Node *node;
	};

	TArray<FName> nodeNames;
	TArray<FrameData> frame;
	Node *pose;

	template<typename T, size_t L> T ParseVector(FScanner &sc);

public:
	~FSMDAnim();
	unsigned int Load(const char* fn, int lumpnum, const char* buffer, int length);
	void SetPose(FSMDModel &model, unsigned int frameno, double inter);
};

#endif
