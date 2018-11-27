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

static FTextureID dummyMtl;

/**
 * Initialize model data to nil
 */
FSMDModel::FSMDModel()
{
}

/**
 * Remove the data that was loaded
 */
FSMDModel::~FSMDModel()
{
	nodelist.Clear();
	tris.Clear();
}

FSMDModel::Node *FSMDModel::GetNodeById(TArray<NodeName> nodeindex, int id)
{
	FString name;

	for (unsigned int i = 0; i < nodeindex.Size(); i++)
	{
		if (nodeindex[i].id == id)
		{
			name = nodeindex[i].name;
			break;
		}
	}

	if (name.IsNotEmpty())
	{
		for (unsigned int i = 0; i < nodelist.Size(); i++)
		{
			if (nodelist[i].name == name)
			{
				return &nodelist[i];
			}
		}
	}

	return nullptr;
}

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

	TArray<NodeName> nodeindex;

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
			Node node;
			while (!sc.CheckString("end"))
			{
				sc.MustGetNumber();
				index = sc.Number;
				sc.MustGetString();
				node.name = sc.String;
				sc.MustGetNumber();
				parent = sc.Number;

				nodeindex.Push(NodeName{index, parent, node.name});
				nodelist.Push(node);
			}

			// Link up parents properly.
		}
		else if (sc.Compare("skeleton"))
		{
			int time = -1;
			Node *nodep = nullptr;
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
					nodep = GetNodeById(nodeindex, sc.Number);
					if (!nodep)
					{
						sc.ScriptError("Reference to undefined node id %i\n", sc.Number);
					}

					Node &node = *nodep;

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
			int weightCount, i, j;
			Triangle tri;
			while (!sc.CheckString("end"))
			{
				sc.MustGetString();

				tri.mat = LoadSkin("", sc.String);

				if (!tri.mat.isValid())
				{
					// Relative to model file path?
					tri.mat = LoadSkin(fn, sc.String);
				}

				if (!tri.mat.isValid())
				{
					sc.ScriptMessage("Material %s not found.", sc.String);
					tri.mat = LoadSkin("", "-NOFLAT-");
				}

				for (i = 0; i < 3; i++)
				{
					sc.MustGetNumber();
					tri.verts[i].node = GetNodeById(nodeindex, sc.Number);

					if (!tri.verts[i].node)
					{
						sc.ScriptError("Reference to undefined node id %i\n", sc.Number);
					}

					tri.verts[i].pos = ParseVector<FVector3,3>(sc);
					tri.verts[i].norm = ParseVector<FVector3,3>(sc);
					tri.verts[i].tex = ParseVector<FVector2,2>(sc);

					/// TODO: process and store weights.
					if (sc.CheckNumber())
					{
						if (sc.Crossed)
						{
							sc.UnGet();
							continue;
						}

						weightCount = sc.Number;
						for (j = 0; j < weightCount; j++)
						{
							sc.MustGetNumber();
							//bone = sc.Number;

							sc.MustGetFloat();
							//weight = sc.Float;
						}
					}
				}

				tris.Push(tri);
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
 * OBJ models are not animated, so this always returns 0
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
void FSMDModel::RenderFrame(FModelRenderer *renderer, FTexture * skin, int frameno, int frameno2, double inter, int translation)
{
	if (tris.Size() == 0)
	{
		return;
	}

	if (!skin)
	{
		/// FIXME: Group triangles by material. @_@;
		if (tris[0].mat.isValid())
		{
			skin = TexMan(tris[0].mat);
		}
		else
		{
			return;
		}
	}
	renderer->SetMaterial(skin, false, translation);
	GetVertexBuffer(renderer)->SetupFrame(renderer, 0, 0, tris.Size() * 3);
	renderer->DrawArrays(0, tris.Size() * 3);
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

	auto vbuf = renderer->CreateVertexBuffer(false,true);
	SetVertexBuffer(renderer, vbuf);

	FModelVertex *vertptr = vbuf->LockVertexBuffer(tris.Size() * 3);

	for (unsigned int i = 0; i < tris.Size(); i++)
	{
		for (unsigned int j = 0; j < 3; j++)
		{
			vertptr->Set(tris[i].verts[j].pos.X, tris[i].verts[j].pos.Z, tris[i].verts[j].pos.Y, tris[i].verts[j].tex.X, -tris[i].verts[j].tex.Y);
			vertptr->SetNormal(tris[i].verts[j].norm.X, tris[i].verts[j].norm.Z, tris[i].verts[j].norm.Y);
			vertptr++;
		}
	}

	vbuf->UnlockVertexBuffer();
}

/**
 * Pre-cache skins for the model
 *
 * @param hitlist The list of textures
 */
void FSMDModel::AddSkins(uint8_t* hitlist)
{
	/// FIXME: Group triangles by material. @_@;
	for (unsigned int i = 0; i < tris.Size(); i++)
	{
		if (tris[i].mat.isValid())
		{
			hitlist[tris[i].mat.GetIndex()] |= FTextureManager::HIT_Flat;
		}
	}
}

