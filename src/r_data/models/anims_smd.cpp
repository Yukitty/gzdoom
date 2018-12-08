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

#include "r_data/models/models_smd.h"

#include "w_wad.h"
#include "sc_man.h"
#include "v_text.h"

FSMDAnim::~FSMDAnim()
{
	for (auto i = frame.begin(); i != frame.end(); i++)
	{
		delete i->node;
	}
	frame.Clear();
}

/**
 * Parse an x-Dimensional vector
 *
 * @tparam T A subclass of TVector2 to be used
 * @tparam L The length of the vector to parse
 */
template<typename T, size_t L> T FSMDAnim::ParseVector(FScanner &sc)
{
	float *coord = new float[L];
	for (size_t axis = 0; axis < L; axis++)
	{
		sc.MustGetFloat();
		coord[axis] = (float)sc.Float;
	}
	T vec(coord);
	delete[] coord;
	return vec;
}

// FIXME: This belongs in another class, not here!
static const FVector4 EulerToQuat(FVector3 euler) // yaw (Z), pitch (Y), roll (X)
{
	// Abbreviations for the various angular functions
	double cy = cos(euler.Z * 0.5);
	double sy = sin(euler.Z * 0.5);
	double cp = cos(euler.Y * 0.5);
	double sp = sin(euler.Y * 0.5);
	double cr = cos(euler.X * 0.5);
	double sr = sin(euler.X * 0.5);

	FVector4 q;
	q.W = (float)(cy * cp * cr + sy * sp * sr);
	q.X = (float)(cy * cp * sr - sy * sp * cr);
	q.Y = (float)(sy * cp * sr + cy * sp * cr);
	q.Z = (float)(sy * cp * cr - cy * sp * sr);
	return q;
}

/**
 * Load a sourcemdl animation.
 *
 * @param fn Path to the model folder
 * @param lumpnum The lump index in the wad collection
 * @param buffer The contents of the animation file
 * @param length Length of buffer
 * @return Number of frames of animation made available by this file or 0 on error
 */
unsigned int FSMDAnim::Load(const char* fn, int lumpnum, const char* buffer, int length)
{
	Node *lastFrame = nullptr;

	FScanner sc;
	FString smdName = Wads.GetLumpFullPath(lumpnum);
	FString smdBuf(buffer, length);

	sc.OpenString(smdName, smdBuf);

	// Format version formalities.
	sc.MustGetStringName("version");
	sc.MustGetNumber();
	if (sc.Number != 1)
	{
		sc.ScriptError("Unsupported format version %u\n", sc.Number);
	}

	TMap<int, unsigned int> index;
	nodeNames.Clear();
	frame.Clear();

	while (sc.GetString())
	{
		if (sc.Compare("nodes"))
		{
			nodeNames.Clear();
			while (!sc.CheckString("end"))
			{
				sc.MustGetNumber();
				index[sc.Number] = nodeNames.Size();

				sc.MustGetString();
				nodeNames.Push(sc.String);

				sc.MustGetNumber();
				// Don't care about parents. (Sorry, Harry.)
			}
		}
		else if (sc.Compare("skeleton"))
		{
			FrameData thisFrame = {0, nullptr};
			frame.Clear();
			while (!sc.CheckString("end"))
			{
				if (sc.CheckString("time"))
				{
					// Store the previously read frame data.
					if (thisFrame.node)
					{
						frame.Push(thisFrame);
					}
					FrameData lastFrame = thisFrame;

					// Allocate new frame data.
					sc.MustGetNumber();
					thisFrame.time = sc.Number;
					thisFrame.node = new Node[nodeNames.Size()];

					// Initialize new frame data to previous frame data, if available.
					if (lastFrame.node)
					{
						memcpy(thisFrame.node, lastFrame.node, sizeof(Node) * nodeNames.Size());
					}
				}
				else if (!thisFrame.node)
				{
					sc.ScriptError("Undefined time in skeleton\n");
				}
				else
				{
					sc.MustGetNumber();
					if (!index.CheckKey(sc.Number))
					{
						sc.ScriptError("Reference to undefined node id %i\n", sc.Number);
					}

					unsigned int nodeIndex = index[sc.Number];

					Node *node = &thisFrame.node[nodeIndex];
					node->pos = ParseVector<FVector3,3>(sc);
					node->rot = EulerToQuat(ParseVector<FVector3,3>(sc));
				}
			}

			// Aaand finally push the last frame. :3
			if (thisFrame.node)
			{
				frame.Push(thisFrame);
			}
		}
		else
		{
			// In this model format we can actually handle unrecognised sections cleanly.
			// Just look for the end.
			sc.ScriptMessage("Unhandled section \"%s\"\n", sc.String);
			while(!sc.CheckString("end"))
			{
				sc.MustGetString();
			}
		}
	}

	return frame.Size();
}

/**
 * Set a sourcemdl Model to the pose of a frame of the loaded animation.
 *
 * @param model FSMDModel to pose
 * @param frameno Local animation frame number to set the pose to
 * @param inter Pose interpolation bias to apply (1.0 to fully overwrite the existing pose)
 */
void FSMDAnim::SetPose(FSMDModel &model, unsigned int frameno, double inter)
{
	if (frameno >= frame.Size())
	{
		Printf(TEXTCOLOR_RED "Invalid frameno %u\n", frameno);
		return;
	}

	Node *node = frame[frameno].node;
	for (unsigned int i = 0; i < nodeNames.Size(); i++)
	{
		if (inter >= 1.0)
		{
			model.nodes[nodeNames[i]].pos = node[i].pos;
			model.nodes[nodeNames[i]].rot = node[i].rot;
		}
		else
		{
			FSMDModel::Node &model_node = model.nodes[nodeNames[i]];
			model_node.pos = node[i].pos * inter + model_node.pos * (1.0 - inter);
			model_node.rot = node[i].rot;
		}
	}
}
