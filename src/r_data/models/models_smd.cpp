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

/**
 * Load an MD5 model
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
			for (unsigned int i = 0, checked = 0; checked < nodeIndex.CountUsed(); i++)
			{
				if (nodeIndex.CheckKey(i))
				{
					++checked;
					if (parents.CheckKey(i))
					{
						nodes[nodeIndex[i]].parent = &nodes[nodeIndex[parents[i]]];
					}
					else
					{
						nodes[nodeIndex[i]].parent = nullptr;
					}
				}
			}
		}
		else if (sc.Compare("skeleton"))
		{
			int time = -1;
			FVector3 pos, rot;
			while (!sc.CheckString("end"))
			{
				if (sc.CheckString("time"))
				{
					sc.MustGetNumber();
					time = sc.Number;
				}
				else if (time == -1)
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

					if (time == 0)
					{
						node.pos = pos;
						node.rot = rot;
					}
				}
			}
		}
		else if (sc.Compare("triangles"))
		{
			FTextureID material;
			Triangle triangle;

			unsigned int i, j, weightCount;
			Node *node;
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

					/// \todo Fully process and store weights.
					if (sc.CheckNumber())
					{
						if (sc.Crossed)
						{
							sc.UnGet();
							continue;
						}

						weightCount = sc.Number;
						totalWeight = 0.0f;
						for (j = 0; j < weightCount; j++)
						{
							sc.MustGetNumber();
							if (!nodeIndex.CheckKey(sc.Number))
							{
								sc.ScriptError("Reference to undefined node id %i\n", sc.Number);
							}
							node = &nodes[nodeIndex[sc.Number]];

							// HACK: If node weights add up to 100% or more, change root bone.
							sc.MustGetFloat();
							totalWeight += sc.Float;
							if (totalWeight > 0.9999f)
							{
								v.node = node;
							}
						}
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
 * Find the index of the frame with the given name
 *
 * @param name The name of the frame
 * @return The index of the frame
 */
int FSMDModel::FindFrame(const char* name)
{
	return 0;
}

/**
 * Render the model
 *
 * @param renderer The model renderer
 * @param skin The loaded skin for the surface
 * @param frameno Unused
 * @param frameno2 Unused
 * @param inter Unused
 * @param translation The translation for the skin
 */
void FSMDModel::RenderFrame(FModelRenderer *renderer, FTexture *skin, int frameno, int frameno2, double inter, int translation)
{
	auto vbuf = GetVertexBuffer(renderer);
	FTexture *useSkin;

	double time = frameno + inter;

	// Build the skeleton.
	// TODO: use frameno, frameno2, and inter to calculate a specific pose.
	TMap<FString, Node> skeleton;
    //FMatrix3x3 (vector.h)
	{
		TMap<FString, Node>::Iterator it(nodes);
		TMap<FString, Node>::Pair *pair;
		while (it.NextPair(pair))
		{
		}
	}

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
					Node &node = *v.node;

                    FVector3 pos(v.pos);
                    /*
					FVector3 pos(skeleton[node.name].pos);

					FVector3 off(v.pos.X - pos.X, v.pos.Y - pos.Y, v.pos.Z - pos.Z);

					// TEST
					if (!node.name.Compare("Head"))
					{
						off *= 0.5;
					}

					pos += off;
					*/

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

