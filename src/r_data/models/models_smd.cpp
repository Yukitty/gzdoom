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

/**
 * Parse an x-Dimensional vector
 *
 * @tparam T A subclass of TVector2 to be used
 * @tparam L The length of the vector to parse
 */
template<typename T, size_t L> T FSMDModel::ParseVector(FScanner &sc)
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
static FVector3 RotateVector3(FVector4 quat, FVector3 vec)
{
	FVector4 q = quat.Unit();
	FVector3 u = q.XYZ();
	return 2 * (u | vec) * u + (pow(q.W, 2) - (u | u)) * vec + 2 * q.W * (u ^ vec);
}

// FIXME: This belongs in another class, not here!
static FVector4 EulerToQuat(FVector3 euler) // yaw (Z), pitch (Y), roll (X)
{
	// Abbreviations for the various angular functions
	double cy = cos(euler.Z * 0.5);
	double sy = sin(euler.Z * 0.5);
	double cp = cos(euler.Y * 0.5);
	double sp = sin(euler.Y * 0.5);
	double cr = cos(euler.X * 0.5);
	double sr = sin(euler.X * 0.5);

	FVector4 q;
	q.W = cy * cp * cr + sy * sp * sr;
	q.X = cy * cp * sr - sy * sp * cr;
	q.Y = sy * cp * sr + cy * sp * cr;
	q.Z = sy * cp * cr - cy * sp * sr;
	return q;
}

// FIXME: This belongs in another class, not here!
static FVector4 InverseQuat(FVector4 quat)
{
	float norm = pow(quat.X, 2) + pow(quat.Y, 2) + pow(quat.Z, 2) + pow(quat.W, 2);
	return FVector4(-quat.X / norm, -quat.Y / norm, -quat.Z / norm, quat.W / norm);
}

/// Calculate the offset of a vertex relative to a bone it's attached to by "un-rotating" it.
FVector3 FSMDModel::CalcVertOff(FVector3 pos, FVector3 bonePos, FVector4 boneRot)
{
	return RotateVector3(InverseQuat(boneRot), pos - bonePos);
}

// FIXME: This belongs in another class, not here!
static FVector4 CombineQuat(FVector4 a, FVector4 b)
{
	return FVector4(
		a.W * b.X + a.X * b.W + a.Y * b.Z - a.Z * b.Y, // X
		a.W * b.Y + a.Y * b.W + a.Z * b.X - a.X * b.Z, // Y
		a.W * b.Z + a.Z * b.W + a.X * b.Y - a.Y * b.X, // Z
		a.W * b.W - a.X * b.X - a.Y * b.Y - a.Z * b.Z // W
	);
}

TMap<FName, FSMDModel::Node> FSMDModel::FlattenSkeleton()
{
	TMap<FName, Node> skeleton;
	skeleton.Clear();
	{
		TMap<FName, Node>::Iterator it(nodes);
		TMap<FName, Node>::Pair *pair;
		while (it.NextPair(pair))
		{
			Node node(pair->Value);
			Node *parent = node.parent;
			while (parent) {
				node.pos = RotateVector3(parent->rot, node.pos) + parent->pos;
				node.rot = CombineQuat(parent->rot, node.rot);
				parent = parent->parent;
			}
			skeleton[pair->Key] = node;
		}
	}
	return skeleton;
}

/**
 * Load a sourcemdl model
 *
 * @param fn The path to the model file
 * @param lumpnum The lump index in the wad collection
 * @param buffer The contents of the model file
 * @param length The size of the model file
 * @return Whether or not the model was parsed successfully
 */
bool FSMDModel::Load(const char* fn, int lumpnum, const char* buffer, int length)
{
	FScanner sc;
	FString smdName = Wads.GetLumpFullPath(lumpnum);
	FString smdBuf(buffer, length);

	TMap<unsigned int, FString> nodeIndex;

	sc.OpenString(smdName, smdBuf);

	// Format version formalities.
	sc.MustGetStringName("version");
	sc.MustGetNumber();
	if (sc.Number != 1)
	{
		sc.ScriptError("Unsupported format version %u\n", sc.Number);
	}

	// Reference pose skeleton calculation.
	TMap<FName, Node> skeleton;

	while (sc.GetString())
	{
		if (sc.Compare("nodes"))
		{
			int index, parent;
			FString name;
			Node node;

			TMap<unsigned int, unsigned int> parents;

			while (!sc.CheckString("end"))
			{
				sc.MustGetNumber();
				index = sc.Number;
				sc.MustGetString();
				name = sc.String;

				nodeIndex.Insert(index, name);

				sc.MustGetNumber();
				if (sc.Number > -1)
				{
					parents.Insert(index, sc.Number);
				}

 				node.name = name;
				nodes.Insert(name, node);
			}

			// Link up parents properly now that all of the bones have been defined.
			{
				TMap<unsigned int, FString>::Iterator it(nodeIndex);
				TMap<unsigned int, FString>::Pair *pair;
				while (it.NextPair(pair))
				{
					if (parents.CheckKey(pair->Key))
					{
						nodes[pair->Value].parent = &nodes[nodeIndex[parents[pair->Key]]];
					}
					else
					{
						nodes[pair->Value].parent = nullptr;
					}
				}
			}
		}
		else if (sc.Compare("skeleton"))
		{
			bool read_pose = false, reference_pose = false;
			FVector3 pos, rot;
			while (!sc.CheckString("end"))
			{
				if (sc.CheckString("time"))
				{
					sc.MustGetNumber();

					if (reference_pose)
					{
						sc.ScriptMessage("Ignoring non reference pose in main sourcemdl file.\n");
						read_pose = false;
					}
					else
					{
						reference_pose = true;
						read_pose = true;
					}
				}
				else if (!reference_pose)
				{
					sc.ScriptError("Undefined time in skeleton\n");
				}
				else
				{
					sc.MustGetNumber();
					if (!nodeIndex.CheckKey(sc.Number))
					{
						sc.ScriptError("Reference to undefined node id %i\n", sc.Number);
					}

					Node &node = nodes[nodeIndex[sc.Number]];
					pos = ParseVector<FVector3,3>(sc);
					rot = ParseVector<FVector3,3>(sc);

					if (read_pose)
					{
						node.pos = pos;
						node.rot = EulerToQuat(rot);
					}
				}
			}

			// Calculate skeleton with full bone positions.
			skeleton = FlattenSkeleton();
		}
		else if (sc.Compare("triangles"))
		{
			FTextureID material;
			Triangle triangle;

			unsigned int i, j, weightCount, w;
			float totalWeight;

			surfaceList.Clear();

			while (!sc.CheckString("end"))
			{
				sc.MustGetString();
				material = LoadSkin("", sc.String);

				if (!material.isValid())
				{
					// Relative to model file path?
					material = LoadSkin(fn, sc.String);
				}

				if (!material.isValid())
				{
					sc.ScriptMessage("Material %s not found.", sc.String);
					material = LoadSkin("", "-NOFLAT-");
				}

				for (i = 0; i < 3; i++)
				{
					Vertex &v = triangle.vertex[i];

					sc.MustGetNumber();
					if (!nodeIndex.CheckKey(sc.Number))
					{
						sc.ScriptError("Reference to undefined node id %i\n", sc.Number);
					}
					v.node = &nodes[nodeIndex[sc.Number]];

					v.pos = ParseVector<FVector3,3>(sc);
					v.normal = ParseVector<FVector3,3>(sc);
					v.texCoord = ParseVector<FVector2,2>(sc);

					// Flip the UV because Doom textures.
					v.texCoord.Y = 1.0 - v.texCoord.Y;

					// Process bone weights
					if (!sc.CheckNumber())
					{
						weightCount = 0;
					}
					else if (sc.Crossed)
					{
						sc.UnGet();
						weightCount = 0;
					}
					else
					{
						weightCount = sc.Number;
					}

					if (weightCount > 8)
					{
						sc.ScriptError("Too many weights on vertex.");
					}

					totalWeight = 0.0f;
					w = 0;
					while (w < weightCount)
					{
						Weight &weight = v.weight[w++];

						sc.MustGetNumber();
						if (!nodeIndex.CheckKey(sc.Number))
						{
							sc.ScriptError("Reference to undefined node id %i\n", sc.Number);
						}
						weight.nodeName = nodeIndex[sc.Number];

						sc.MustGetFloat();
						weight.bias = sc.Float;
						totalWeight += sc.Float;

						// HACK: If node weights add up to 100% or more, change root bone.
						if (totalWeight > 0.9999f)
						{
							v.node = &nodes[weight.nodeName];
						}

						if (totalWeight > 1.001f)
						{
							sc.ScriptMessage("Too much weight on vertex.");
						}

						// Calculate vertex offset based on reference skeleton.
						weight.pos = CalcVertOff(v.pos, skeleton[weight.nodeName].pos, skeleton[weight.nodeName].rot);
					}

					if (totalWeight < 1.0f && w < 8)
					{
						Weight &weight = v.weight[w++];
						weight.nodeName = v.node->name;
						weight.bias = 1.0f - totalWeight;
						weight.pos = CalcVertOff(v.pos, skeleton[weight.nodeName].pos, skeleton[weight.nodeName].rot);
					}

					while (w < 8)
					{
						Weight &weight = v.weight[w++];
						weight.nodeName = "";
						weight.bias = 0.0f;
						weight.pos = FVector3(0, 0, 0);
					}
				}

				// Find an existing surface with the matching material.
				Surface *surface = nullptr;
				for (auto i = surfaceList.begin(); i != surfaceList.end(); i++)
				{
					if (i->material == material)
					{
						surface = &(*i);
						break;
					}
				}

				// New material? Add a new surface!
				if (!surface)
				{
					surface = &surfaceList[surfaceList.Reserve(1)];
					surface->material = material;
				}

				surface->triangle.Push(triangle);
			}
		}
		else
		{
			// In this model format we can actually handle unrecognised sections cleanly.
			// Just look for the end.
			sc.ScriptMessage("Unrecognised section \"%s\"\n", sc.String);
			while(!sc.CheckString("end"))
			{
				sc.MustGetString();
			}
		}
	}
	sc.Close();

	return true;
}

/**
 * Load a sourcemdl-compatible animation
 *
 * @param path The path to the animation file
 * @param name The friendly name of the animation or an empty string
 * @param lumpnum The lump index in the wad collection
 */
void FSMDModel::LoadAnim(const char *path, const char *name, int lumpnum)
{
	// Assume .smd animation file.

	int len = Wads.LumpLength(lumpnum);
	FMemLump lumpd = Wads.ReadLump(lumpnum);
	char * buffer = (char*)lumpd.GetMem();

	unsigned int index = animList.Reserve(1);
	Animation &anim = animList[index];
	anim.start = frameCount;
	anim.frames = anim.data.Load(path, lumpnum, buffer, len);
	frameCount += anim.frames;

	if (FString(name).Len() > 0)
	{
		animNameIndex[name] = index;
		Printf("Loaded %u frames for animation %s\n", anim.frames, name);
	}
}

/**
 * Find the index of the frame with the given name
 *
 * @param name The name of the frame
 * @return The index of the frame
 */
int FSMDModel::FindFrame(const char* name)
{
	/// \todo Search and return by anim names + frame numbers
	return 0;
}

/**
 * Render the model
 *
 * @param renderer The model renderer
 * @param skin The loaded skin for the surface
 * @param frameno Current animation keyframe
 * @param frameno2 Animation blend keyframe
 * @param inter Interpolation bias towards frameno2
 * @param translation The translation for the skin
 */
void FSMDModel::RenderFrame(FModelRenderer *renderer, FTexture *skin, int frameno, int frameno2, double inter, int translation)
{
	auto vbuf = GetVertexBuffer(renderer);
	FTexture *useSkin;

	// Find animation + frame from global frameno.
	for (auto a = animList.begin(); a != animList.end(); a++)
	{
		if ((unsigned)frameno >= a->start && (unsigned)frameno < a->start + a->frames)
		{
			// Pose the model by animation.
			a->data.SetPose(*this, frameno - a->start, 1.0);
			break;
		}
	}

	// Build the skeleton.
	TMap<FName, Node> skeleton = FlattenSkeleton();

	// Build vertex buffer.
	// Yes, every frame.
	// Lord help me.
	{
		FModelVertex *vertp = vbuf->LockVertexBuffer(vbufSize);

		for (auto s = surfaceList.begin(); s != surfaceList.end(); s++)
		{
			for (auto t = s->triangle.begin(); t != s->triangle.end(); t++)
			{
				for (unsigned int i = 0; i < 3; i++)
				{
					Vertex &v = t->vertex[i];
					FVector3 pos(0, 0, 0);
					for (unsigned int w = 0; w < 8; w++)
					{
						Weight &weight = v.weight[w];
						if (weight.bias <= 0.0f)
						{
							continue;
						}

						Node *node = skeleton.CheckKey(weight.nodeName);
						if (node)
						{
							pos += (node->pos + RotateVector3(node->rot, weight.pos)) * weight.bias;
						}
					}

					vertp->Set(pos.X, pos.Z, pos.Y, v.texCoord.X, v.texCoord.Y);
					vertp->SetNormal(v.normal.X, v.normal.Z, v.normal.Y);
					vertp++;
				}
			}
		}
	}
	vbuf->UnlockVertexBuffer();

	// Render surfaces.
	unsigned int start = 0;
	for (auto s = surfaceList.begin(); s != surfaceList.end(); s++)
	{
		useSkin = skin;

		if (!useSkin)
		{
			if (s->material.isValid())
			{
				useSkin = TexMan(s->material);
			}
			else
			{
				// invalid texture, nothing to render.
				start += s->triangle.Size() * 3;
				continue;
			}
		}

		renderer->SetMaterial(useSkin, false, translation);
		vbuf->SetupFrame(renderer, start, start, s->triangle.Size() * 3);
		renderer->DrawArrays(0, s->triangle.Size() * 3);

		start += s->triangle.Size() * 3;
	}
}

/**
 * Construct the vertex buffer for this model
 *
 * @param renderer A pointer to the model renderer. Used to allocate the vertex buffer.
 */
void FSMDModel::BuildVertexBuffer(FModelRenderer *renderer)
{
	if (GetVertexBuffer(renderer))
	{
		return;
	}

	// Allocate vertex buffer.
	auto vbuf = renderer->CreateVertexBuffer(false,true);
	SetVertexBuffer(renderer, vbuf);

	// Calculate total vertex buffer size.
	vbufSize = 0;
	for (auto s = surfaceList.begin(); s != surfaceList.end(); s++)
	{
		vbufSize += s->triangle.Size() * 3;
	}
}

/**
 * Pre-cache skins for the model
 *
 * @param hitlist The list of textures
 */
void FSMDModel::AddSkins(uint8_t* hitlist)
{
	for (auto s = surfaceList.begin(); s != surfaceList.end(); s++)
	{
		if (s->material.isValid())
		{
			hitlist[s->material.GetIndex()] |= FTextureManager::HIT_Flat;
		}
	}
}

