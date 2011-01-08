/*******************************************************************************
Copyright (c) 2005-2009 David Williams

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
    claim that you wrote the original software. If you use this software
    in a product, an acknowledgment in the product documentation would be
    appreciated but is not required.

    2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.

    3. This notice may not be removed or altered from any source
    distribution.
*******************************************************************************/

namespace PolyVox
{
	////////////////////////////////////////////////////////////////////////////////
	/// Builds a MeshDecimator.
	/// \param pInputMesh A pointer to the mesh to be decimated.
	/// \param pOutputMesh A pointer to where the result should be stored. Any existing
	/// contents will be deleted.
	/// \param fEdgeCollapseThreshold This is only use in the case of a Marching Cubes
	/// surface and controls how close two normals must be to collapse. The dot product
	/// between the normals is computed and compared to this threshold. A threshold of
	/// 1.0 means nothing will collapse, a threshold of 0.0 means everything will collapse.
	////////////////////////////////////////////////////////////////////////////////
	template <typename VertexType>
	MeshDecimator<VertexType>::MeshDecimator(const SurfaceMesh<VertexType>* pInputMesh, SurfaceMesh<VertexType>* pOutputMesh, float fEdgeCollapseThreshold)
		:m_pInputMesh(pInputMesh)
		,m_pOutputMesh(pOutputMesh)
		,m_fMinDotProductForCollapse(fEdgeCollapseThreshold)
	{
		*m_pOutputMesh = *m_pInputMesh;
	}

	template <typename VertexType>
	void MeshDecimator<VertexType>::execute()
	{
		//Sanity check.
		if((m_pOutputMesh->m_vecVertices.empty()) || (m_pOutputMesh->m_vecTriangleIndices.empty()))
		{
			return;
		}

		buildConnectivityData();
		fillInitialVertexMetadata(m_vecInitialVertexMetadata);

		uint32_t noOfEdgesCollapsed;
		do
		{
			noOfEdgesCollapsed = performDecimationPass(m_fMinDotProductForCollapse);
			m_pOutputMesh->removeDegenerateTris();	
			if(noOfEdgesCollapsed > 0)
			{
				//Build the connectivity data for the next pass. If this is slow, then look
				//at adjusting it (based on vertex mapper?) rather than bulding from scratch.
				buildConnectivityData();
			}
		}while(noOfEdgesCollapsed > 0);

		m_pOutputMesh->removeUnusedVertices();

		//Decimation will have invalidated LOD levels.
		m_pOutputMesh->m_vecLodRecords.clear();
		LodRecord lodRecord;
		lodRecord.beginIndex = 0;
		lodRecord.endIndex = m_pOutputMesh->getNoOfIndices();
		m_pOutputMesh->m_vecLodRecords.push_back(lodRecord);
	}

	template <typename VertexType>
	void MeshDecimator<VertexType>::buildConnectivityData(void)
	{
		//Build a list of all the triangles, complete with face normals.
		m_vecTriangles.clear();
		m_vecTriangles.resize(m_pOutputMesh->m_vecTriangleIndices.size() / 3);
		for(int triCt = 0; triCt < m_vecTriangles.size(); triCt++)
		{
			m_vecTriangles[triCt].v0 = m_pOutputMesh->m_vecTriangleIndices[triCt * 3 + 0];
			m_vecTriangles[triCt].v1 = m_pOutputMesh->m_vecTriangleIndices[triCt * 3 + 1];
			m_vecTriangles[triCt].v2 = m_pOutputMesh->m_vecTriangleIndices[triCt * 3 + 2];

			Vector3DFloat v0Pos = m_pOutputMesh->m_vecVertices[m_vecTriangles[triCt].v0].position;
			Vector3DFloat v1Pos = m_pOutputMesh->m_vecVertices[m_vecTriangles[triCt].v1].position;
			Vector3DFloat v2Pos = m_pOutputMesh->m_vecVertices[m_vecTriangles[triCt].v2].position;

			Vector3DFloat v0v1 = v1Pos - v0Pos;
			Vector3DFloat v0v2 = v2Pos - v0Pos;
			Vector3DFloat normal = v0v1.cross(v0v2);
			normal.normalise();

			m_vecTriangles[triCt].normal = normal;
		}

		//For each vertex, determine which triangles are using it.
		trianglesUsingVertex.clear();
		trianglesUsingVertex.resize(m_pOutputMesh->m_vecVertices.size());
		for(int ct = 0; ct < trianglesUsingVertex.size(); ct++)
		{
			trianglesUsingVertex[ct].reserve(6);
		}
		for(int ct = 0; ct < m_vecTriangles.size(); ct++)
		{
			trianglesUsingVertex[m_vecTriangles[ct].v0].push_back(ct);
			trianglesUsingVertex[m_vecTriangles[ct].v1].push_back(ct);
			trianglesUsingVertex[m_vecTriangles[ct].v2].push_back(ct);
		}
	}

	template<>
	void MeshDecimator<PositionMaterial>::fillInitialVertexMetadata(std::vector<InitialVertexMetadata>& vecVertexMetadata)
	{
		vecVertexMetadata.clear();
		vecVertexMetadata.resize(m_pOutputMesh->m_vecVertices.size());
		//Initialise the metadata
		for(int ct = 0; ct < vecVertexMetadata.size(); ct++)
		{
			vecVertexMetadata[ct].normal.setElements(0,0,0);
			vecVertexMetadata[ct].isOnMaterialEdge = false;
			vecVertexMetadata[ct].isOnRegionFace.reset();
		}

		//Identify duplicate vertices, as they lie on the material edge. To do this we convert into integers and sort 
		//(first on z, then y, then x). They should be mostly in order as this is the order they come out of the
		//CubicSurfaceExtractor in. Duplicates are now neighbours in the resulting list so just scan through for pairs.
		std::vector<IntVertex> intVertices;
		intVertices.reserve(m_pOutputMesh->m_vecVertices.size());
		for(int ct = 0; ct < m_pOutputMesh->m_vecVertices.size(); ct++)
		{
			const Vector3DFloat& floatPos = m_pOutputMesh->m_vecVertices[ct].position;
			IntVertex intVertex(static_cast<uint32_t>(floatPos.getX()), static_cast<uint32_t>(floatPos.getY()), static_cast<uint32_t>(floatPos.getZ()), ct);
			intVertices.push_back(intVertex);
		}

		//Do the sorting so that duplicate become neighbours
		sort(intVertices.begin(), intVertices.end());

		//Find neighbours which are duplicates.
		for(int ct = 0; ct < intVertices.size() - 1; ct++)
		{
			const IntVertex& v0 = intVertices[ct+0];
			const IntVertex& v1 = intVertices[ct+1];

			if((v0.x == v1.x) && (v0.y == v1.y) && (v0.z == v1.z))
			{
				vecVertexMetadata[v0.index].isOnMaterialEdge = true;
				vecVertexMetadata[v1.index].isOnMaterialEdge = true;
			}
		}

		//Compute an approcimation to the normal, used when deciding if an edge can collapse.
		for(int ct = 0; ct < m_pOutputMesh->m_vecVertices.size(); ct++)
		{
			Vector3DFloat sumOfNormals(0.0f,0.0f,0.0f);
			for(vector<uint32_t>::const_iterator iter = trianglesUsingVertex[ct].cbegin(); iter != trianglesUsingVertex[ct].cend(); iter++)
			{
				sumOfNormals += m_vecTriangles[*iter].normal;
			}

			vecVertexMetadata[ct].normal = sumOfNormals;
			vecVertexMetadata[ct].normal.normalise();
		}

		//Identify those vertices on the edge of a region. Care will need to be taken when moving them.
		for(int ct = 0; ct < vecVertexMetadata.size(); ct++)
		{
			Region regTransformed = m_pOutputMesh->m_Region;
			regTransformed.shift(regTransformed.getLowerCorner() * static_cast<int16_t>(-1));

			//Plus and minus X
			vecVertexMetadata[ct].isOnRegionFace.set(RFF_ON_REGION_FACE_NEG_X, m_pOutputMesh->m_vecVertices[ct].getPosition().getX() < regTransformed.getLowerCorner().getX() + 0.001f);
			vecVertexMetadata[ct].isOnRegionFace.set(RFF_ON_REGION_FACE_POS_X, m_pOutputMesh->m_vecVertices[ct].getPosition().getX() > regTransformed.getUpperCorner().getX() - 0.001f);
			//Plus and minus Y
			vecVertexMetadata[ct].isOnRegionFace.set(RFF_ON_REGION_FACE_NEG_Y, m_pOutputMesh->m_vecVertices[ct].getPosition().getY() < regTransformed.getLowerCorner().getY() + 0.001f);
			vecVertexMetadata[ct].isOnRegionFace.set(RFF_ON_REGION_FACE_POS_Y, m_pOutputMesh->m_vecVertices[ct].getPosition().getY() > regTransformed.getUpperCorner().getY() - 0.001f);
			//Plus and minus Z
			vecVertexMetadata[ct].isOnRegionFace.set(RFF_ON_REGION_FACE_NEG_Z, m_pOutputMesh->m_vecVertices[ct].getPosition().getZ() < regTransformed.getLowerCorner().getZ() + 0.001f);
			vecVertexMetadata[ct].isOnRegionFace.set(RFF_ON_REGION_FACE_POS_Z, m_pOutputMesh->m_vecVertices[ct].getPosition().getZ() > regTransformed.getUpperCorner().getZ() - 0.001f);
		}
	}

	template<>
	void MeshDecimator<PositionMaterialNormal>::fillInitialVertexMetadata(std::vector<InitialVertexMetadata>& vecVertexMetadata)
	{
		vecVertexMetadata.clear();
		vecVertexMetadata.resize(m_pOutputMesh->m_vecVertices.size());

		//Initialise the metadata
		for(int ct = 0; ct < vecVertexMetadata.size(); ct++)
		{			
			vecVertexMetadata[ct].isOnRegionFace.reset();
			vecVertexMetadata[ct].isOnMaterialEdge = false;
			vecVertexMetadata[ct].normal = m_pOutputMesh->m_vecVertices[ct].normal;
		}

		//Identify those vertices on the edge of a region. Care will need to be taken when moving them.
		for(int ct = 0; ct < vecVertexMetadata.size(); ct++)
		{
			Region regTransformed = m_pOutputMesh->m_Region;
			regTransformed.shift(regTransformed.getLowerCorner() * static_cast<int16_t>(-1));

			//Plus and minus X
			vecVertexMetadata[ct].isOnRegionFace.set(RFF_ON_REGION_FACE_NEG_X, m_pOutputMesh->m_vecVertices[ct].getPosition().getX() < regTransformed.getLowerCorner().getX() + 0.001f);
			vecVertexMetadata[ct].isOnRegionFace.set(RFF_ON_REGION_FACE_POS_X, m_pOutputMesh->m_vecVertices[ct].getPosition().getX() > regTransformed.getUpperCorner().getX() - 0.001f);
			//Plus and minus Y
			vecVertexMetadata[ct].isOnRegionFace.set(RFF_ON_REGION_FACE_NEG_Y, m_pOutputMesh->m_vecVertices[ct].getPosition().getY() < regTransformed.getLowerCorner().getY() + 0.001f);
			vecVertexMetadata[ct].isOnRegionFace.set(RFF_ON_REGION_FACE_POS_Y, m_pOutputMesh->m_vecVertices[ct].getPosition().getY() > regTransformed.getUpperCorner().getY() - 0.001f);
			//Plus and minus Z
			vecVertexMetadata[ct].isOnRegionFace.set(RFF_ON_REGION_FACE_NEG_Z, m_pOutputMesh->m_vecVertices[ct].getPosition().getZ() < regTransformed.getLowerCorner().getZ() + 0.001f);
			vecVertexMetadata[ct].isOnRegionFace.set(RFF_ON_REGION_FACE_POS_Z, m_pOutputMesh->m_vecVertices[ct].getPosition().getZ() > regTransformed.getUpperCorner().getZ() - 0.001f);
		}

		//If all three vertices have the same material then we are not on a material edge. If any vertex has a different
		//material then all three vertices are on a material edge. E.g. If one vertex has material 'a' and the other two 
		//have material 'b', then the two 'b's are still on an edge (with 'a') even though they are the same as eachother.
		for(int ct = 0; ct < m_vecTriangles.size(); ct++)
		{
			uint32_t v0 = m_vecTriangles[ct].v0;
			uint32_t v1 = m_vecTriangles[ct].v1;
			uint32_t v2 = m_vecTriangles[ct].v2;

			bool allMatch = 
				(m_pOutputMesh->m_vecVertices[v0].material == m_pOutputMesh->m_vecVertices[v1].material) && 
				(m_pOutputMesh->m_vecVertices[v1].material == m_pOutputMesh->m_vecVertices[v2].material);

			if(!allMatch)
			{
				vecVertexMetadata[v0].isOnMaterialEdge = true;
				vecVertexMetadata[v1].isOnMaterialEdge = true;
				vecVertexMetadata[v2].isOnMaterialEdge = true;
			}
		}
	}

	template <typename VertexType>
	uint32_t MeshDecimator<VertexType>::performDecimationPass(float m_fMinDotProductForCollapse)
	{
		// Count how many edges we have collapsed
		uint32_t noOfEdgesCollapsed = 0;

		// The vertex mapper track whick vertices collapse onto which.
		vertexMapper.clear();
		vertexMapper.resize(m_pOutputMesh->m_vecVertices.size());

		// Once a vertex is involved in a collapse (either because it
		// moves onto a different vertex, or because a different vertex
		// moves onto it) it is forbidden to take part in another collapse
		// this pass. We enforce this by setting the vertex locked flag.
		vertexLocked.clear();
		vertexLocked.resize(m_pOutputMesh->m_vecVertices.size());

		// Initialise the vectors
		for(uint32_t ct = 0; ct < m_pOutputMesh->m_vecVertices.size(); ct++)
		{
			// Initiall all vertices points to themselves
			vertexMapper[ct] = ct;
			// All vertices are initially unlocked
			vertexLocked[ct] = false;
		}

		//For each triangle...
		for(int ctIter = 0; ctIter < m_vecTriangles.size(); ctIter++)
		{
			if(attemptEdgeCollapse(m_vecTriangles[ctIter].v0, m_vecTriangles[ctIter].v1))
			{
				++noOfEdgesCollapsed;
			}

			if(attemptEdgeCollapse(m_vecTriangles[ctIter].v1, m_vecTriangles[ctIter].v2))
			{
				++noOfEdgesCollapsed;
			}

			if(attemptEdgeCollapse(m_vecTriangles[ctIter].v2, m_vecTriangles[ctIter].v0))
			{
				++noOfEdgesCollapsed;
			}
		}

		if(noOfEdgesCollapsed > 0)
		{
			//Fix up the indices
			for(int triCt = 0; triCt < m_pOutputMesh->m_vecTriangleIndices.size(); triCt++)
			{
				uint32_t before = m_pOutputMesh->m_vecTriangleIndices[triCt];
				uint32_t after = vertexMapper[m_pOutputMesh->m_vecTriangleIndices[triCt]];
				if(before != after)
				{
					m_pOutputMesh->m_vecTriangleIndices[triCt] = vertexMapper[m_pOutputMesh->m_vecTriangleIndices[triCt]];
				}
			}
		}

		return noOfEdgesCollapsed;
	}

	template <typename VertexType>
	bool MeshDecimator<VertexType>::attemptEdgeCollapse(uint32_t uSrc, uint32_t uDst)
	{
		//A vertex will be locked if it has already been involved in a collapse this pass.
		if(vertexLocked[uSrc] || vertexLocked[uDst])
		{
			return false;
		}

		if(canCollapseEdge(uSrc, uDst))
		{
			//Move v0 onto v1
			vertexMapper[uSrc] = uDst; //vertexMapper[v1];
			vertexLocked[uSrc] = true;
			vertexLocked[uDst] = true;

			//Increment the counter
			return true;
		}
		
		return false;
	}

	template <typename VertexType>
	bool MeshDecimator<VertexType>::canCollapseEdge(uint32_t uSrc, uint32_t uDst)
	{
		bool bCanCollapse = true;
		
		if(m_vecInitialVertexMetadata[uSrc].isOnMaterialEdge)
		{
			bCanCollapse &= canCollapseMaterialEdge(uSrc, uDst);
		}

		if(m_vecInitialVertexMetadata[uSrc].isOnRegionFace.any())
		{
			bCanCollapse &= canCollapseRegionEdge(uSrc, uDst);
		}

		if(bCanCollapse) //Only bother with this if the earlier tests passed.
		{
			bCanCollapse &= canCollapseNormalEdge(uSrc, uDst);
		}

		return bCanCollapse;
	}

	template<> 
	bool MeshDecimator<PositionMaterialNormal>::canCollapseNormalEdge(uint32_t uSrc, uint32_t uDst)
	{
		if(m_vecInitialVertexMetadata[uSrc].normal.dot(m_vecInitialVertexMetadata[uDst].normal) < m_fMinDotProductForCollapse)
		{
			return false;
		}

		//With the marching cubes surface we honour the user specified threshold
		return !collapseChangesFaceNormals(uSrc, uDst, m_fMinDotProductForCollapse);
	}

	template<> 
	bool MeshDecimator<PositionMaterial>::canCollapseNormalEdge(uint32_t uSrc, uint32_t uDst)
	{
		//We don't actually use the normal here, because we want to allow face
		//vertices to collapse onto edge vertices. Simply checking whether anything
		//has flipped has proved to be the most robust approach, though rather slow.
		//It's not sufficient to just check the normals, there can be holes in the middle
		//of the mesh for example.

		//User specified threshold is not used for cubic surface, any
		//movement is too much (but allow for floating point error).
		return !collapseChangesFaceNormals(uSrc, uDst, 0.999f);
	}

	template <typename VertexType>
	bool MeshDecimator<VertexType>::canCollapseRegionEdge(uint32_t uSrc, uint32_t uDst)
	{		
		// We can collapse normal vertices onto edge vertices, and edge vertices
		// onto corner vertices, but not vice-versa. Hence we check whether all
		// the edge flags in the source vertex are also set in the destination vertex.
		if(isSubset(m_vecInitialVertexMetadata[uSrc].isOnRegionFace, m_vecInitialVertexMetadata[uDst].isOnRegionFace) == false)
		{
			return false;
		}

		// In general adjacent regions surface meshes may collapse differently
		// and this can cause cracks. We solve this by only allowing the collapse
		// is the normals are exactly the same. We do not use the user provided
		// tolerence here (but do allow for floating point error).
		if(m_vecInitialVertexMetadata[uSrc].normal.dot(m_vecInitialVertexMetadata[uDst].normal) < 0.999f)
		{
			return false;
		}

		return true;
	}

	template <typename VertexType>
	bool MeshDecimator<VertexType>::canCollapseMaterialEdge(uint32_t uSrc, uint32_t uDst)
	{
		return false;
	}

	//This function should really use some work. For a start we already have the
	//faces normals for the input mesh yet we are computing them on the fly here.
	template <typename VertexType>
	bool MeshDecimator<VertexType>::collapseChangesFaceNormals(uint32_t uSrc, uint32_t uDst, float fThreshold)
	{
		bool faceFlipped = false;
		vector<uint32_t>& triangles = trianglesUsingVertex[uSrc];

		for(vector<uint32_t>::iterator triIter = triangles.begin(); triIter != triangles.end(); triIter++)
		{
			uint32_t tri = *triIter;
					
			const uint32_t& v0Old = m_pOutputMesh->m_vecTriangleIndices[tri * 3];
			const uint32_t& v1Old = m_pOutputMesh->m_vecTriangleIndices[tri * 3 + 1];
			const uint32_t& v2Old = m_pOutputMesh->m_vecTriangleIndices[tri * 3 + 2];

			//Check if degenerate
			if((v0Old == v1Old) || (v1Old == v2Old) || (v2Old == v0Old))
			{
				continue;
			}

			uint32_t v0New = v0Old;
			uint32_t v1New = v1Old;
			uint32_t v2New = v2Old;

			if(v0New == uSrc)
				v0New = uDst;
			if(v1New == uSrc)
				v1New = uDst;
			if(v2New == uSrc)
				v2New = uDst;

			//Check if degenerate
			if((v0New == v1New) || (v1New == v2New) || (v2New == v0New))
			{
				continue;
			}

			const Vector3DFloat& v0OldPos = m_pOutputMesh->m_vecVertices[vertexMapper[v0Old]].getPosition(); //Note: we need the vertex mapper here. These neighbouring vertices may have been moved.
			const Vector3DFloat& v1OldPos = m_pOutputMesh->m_vecVertices[vertexMapper[v1Old]].getPosition();
			const Vector3DFloat& v2OldPos = m_pOutputMesh->m_vecVertices[vertexMapper[v2Old]].getPosition();

			const Vector3DFloat& v0NewPos = m_pOutputMesh->m_vecVertices[vertexMapper[v0New]].getPosition();
			const Vector3DFloat& v1NewPos = m_pOutputMesh->m_vecVertices[vertexMapper[v1New]].getPosition();
			const Vector3DFloat& v2NewPos = m_pOutputMesh->m_vecVertices[vertexMapper[v2New]].getPosition();

			Vector3DFloat OldNormal = (v1OldPos - v0OldPos).cross(v2OldPos - v1OldPos);
			Vector3DFloat NewNormal = (v1NewPos - v0NewPos).cross(v2NewPos - v1NewPos);

			OldNormal.normalise();
			NewNormal.normalise();

			float dotProduct = OldNormal.dot(NewNormal);
			//NOTE: I don't think we should be using the threshold here, we're just checking for a complete face flip
			if(dotProduct < fThreshold)
			{
				//cout << "   Face flipped!!" << endl;

				faceFlipped = true;

				/*vertexLocked[v0] = true;
				vertexLocked[v1] = true;*/

				break;
			}
		}

		return faceFlipped;
	}

	// Returns true if every bit which is set in 'a' is also set in 'b'. The reverse does not need to be true.
	template <typename VertexType>
	bool MeshDecimator<VertexType>::isSubset(std::bitset<RFF_NO_OF_REGION_FACE_FLAGS> a, std::bitset<RFF_NO_OF_REGION_FACE_FLAGS> b)
	{
		bool result = true;

		for(int ct = 0; ct < RFF_NO_OF_REGION_FACE_FLAGS; ct++)
		{
			if(a.test(ct))
			{
				if(b.test(ct) == false)
				{
					result = false;
					break;
				}
			}
		}

		return result;
	}
}