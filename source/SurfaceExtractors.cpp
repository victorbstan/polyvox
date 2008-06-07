#include "SurfaceExtractors.h"

#include "BlockVolume.h"
#include "GradientEstimators.h"
#include "IndexedSurfacePatch.h"
#include "MarchingCubesTables.h"
#include "Region.h"
#include "RegionGeometry.h"
#include "VolumeChangeTracker.h"
#include "BlockVolumeIterator.h"

#include <algorithm>

using namespace boost;

namespace PolyVox
{
	std::list<RegionGeometry> getChangedRegionGeometry(VolumeChangeTracker& volume)
	{
		std::list<Region> listChangedRegions;
		volume.getChangedRegions(listChangedRegions);

		std::list<RegionGeometry> listChangedRegionGeometry;
		for(std::list<Region>::const_iterator iterChangedRegions = listChangedRegions.begin(); iterChangedRegions != listChangedRegions.end(); ++iterChangedRegions)
		{
			//Generate the surface
			RegionGeometry regionGeometry;
			regionGeometry.m_patchSingleMaterial = new IndexedSurfacePatch(false);
			regionGeometry.m_v3dRegionPosition = iterChangedRegions->getLowerCorner();

			generateExperimentalMeshDataForRegion(volume.getVolumeData(), *iterChangedRegions, regionGeometry.m_patchSingleMaterial);

			//genMultiFromSingle(regionGeometry.m_patchSingleMaterial, regionGeometry.m_patchMultiMaterial);

			regionGeometry.m_bContainsSingleMaterialPatch = regionGeometry.m_patchSingleMaterial->getVertices().size() > 0;
			regionGeometry.m_bIsEmpty = (regionGeometry.m_patchSingleMaterial->getVertices().size() == 0) || (regionGeometry.m_patchSingleMaterial->getIndices().size() == 0);

			listChangedRegionGeometry.push_back(regionGeometry);
		}

		return listChangedRegionGeometry;
	}

	boost::uint32_t getIndex(boost::uint32_t x, boost::uint32_t y)
	{
		return x + (y * (POLYVOX_REGION_SIDE_LENGTH+1));
	}

	void generateExperimentalMeshDataForRegion(BlockVolume<uint8_t>* volumeData, Region region, IndexedSurfacePatch* singleMaterialPatch)
	{	
		singleMaterialPatch->m_vecVertices.clear();
		singleMaterialPatch->m_vecTriangleIndices.clear();

		//For edge indices
		boost::int32_t* vertexIndicesX0 = new boost::int32_t[(POLYVOX_REGION_SIDE_LENGTH+1) * (POLYVOX_REGION_SIDE_LENGTH+1)];
		boost::int32_t* vertexIndicesY0 = new boost::int32_t[(POLYVOX_REGION_SIDE_LENGTH+1) * (POLYVOX_REGION_SIDE_LENGTH+1)];
		boost::int32_t* vertexIndicesZ0 = new boost::int32_t[(POLYVOX_REGION_SIDE_LENGTH+1) * (POLYVOX_REGION_SIDE_LENGTH+1)];
		boost::int32_t* vertexIndicesX1 = new boost::int32_t[(POLYVOX_REGION_SIDE_LENGTH+1) * (POLYVOX_REGION_SIDE_LENGTH+1)];
		boost::int32_t* vertexIndicesY1 = new boost::int32_t[(POLYVOX_REGION_SIDE_LENGTH+1) * (POLYVOX_REGION_SIDE_LENGTH+1)];
		boost::int32_t* vertexIndicesZ1 = new boost::int32_t[(POLYVOX_REGION_SIDE_LENGTH+1) * (POLYVOX_REGION_SIDE_LENGTH+1)];
		memset(vertexIndicesX0,0xFF,sizeof(vertexIndicesX0)); //0xFF is -1 as two's complement - this may not be portable...
		memset(vertexIndicesY0,0xFF,sizeof(vertexIndicesY0));
		memset(vertexIndicesZ0,0xFF,sizeof(vertexIndicesZ0));	
		memset(vertexIndicesX1,0xFF,sizeof(vertexIndicesX1)); //0xFF is -1 as two's complement - this may not be portable...
		memset(vertexIndicesY1,0xFF,sizeof(vertexIndicesY1));
		memset(vertexIndicesZ1,0xFF,sizeof(vertexIndicesZ1));

		//When generating the mesh for a region we actually look one voxel outside it in the
		// back, bottom, right direction. Protect against access violations by cropping region here
		Region regVolume = volumeData->getEnclosingRegion();
		regVolume.setUpperCorner(regVolume.getUpperCorner() - Vector3DInt32(1,1,1));
		region.cropTo(regVolume);

		//Offset from region corner
		const Vector3DFloat offset = static_cast<Vector3DFloat>(region.getLowerCorner());

		//Cell bitmasks
		//boost::uint8_t bitmask0[(POLYVOX_REGION_SIDE_LENGTH+1) * (POLYVOX_REGION_SIDE_LENGTH+1)];
		boost::uint8_t* bitmask0 = new boost::uint8_t[(POLYVOX_REGION_SIDE_LENGTH+1) * (POLYVOX_REGION_SIDE_LENGTH+1)];
		//boost::uint8_t bitmask1[(POLYVOX_REGION_SIDE_LENGTH+1) * (POLYVOX_REGION_SIDE_LENGTH+1)];
		boost::uint8_t* bitmask1 = new boost::uint8_t[(POLYVOX_REGION_SIDE_LENGTH+1) * (POLYVOX_REGION_SIDE_LENGTH+1)];
		memset(bitmask0, 0x00, sizeof(bitmask0));
		memset(bitmask1, 0x00, sizeof(bitmask1));

		Vector3DFloat vertlist[12];
		uint8_t vertMaterials[12];

		Region regFirstSlice(region);
		regFirstSlice.setUpperCorner(Vector3DInt32(regFirstSlice.getUpperCorner().getX(),regFirstSlice.getUpperCorner().getY(),regFirstSlice.getLowerCorner().getZ()));
		
		BlockVolumeIterator<boost::uint8_t> volIter(*volumeData);		

		boost::uint32_t uNoOfNonEmptyCellsForSlice0 = computeBitmaskForSlice(volIter, regFirstSlice, offset, bitmask0);		

		if(uNoOfNonEmptyCellsForSlice0 != 0)
		{
			generateVerticesForSlice(volIter,regFirstSlice, offset, bitmask0, singleMaterialPatch, vertexIndicesX0, vertexIndicesY0, vertexIndicesZ0, /*regTwoSlice.getUpperCorner(),*/ vertlist, vertMaterials);
		}

		for(boost::uint32_t uSlice = 0; ((uSlice <= 15) && (uSlice + offset.getZ() < region.getUpperCorner().getZ())); ++uSlice)
		{
			Vector3DInt32 lowerCorner = Vector3DInt32(region.getLowerCorner().getX(), region.getLowerCorner().getY(), region.getLowerCorner().getZ() + uSlice);
			Vector3DInt32 upperCorner = Vector3DInt32(region.getUpperCorner().getX(), region.getUpperCorner().getY(), region.getLowerCorner().getZ() + uSlice + 1);
			Region regTwoSlice(lowerCorner, upperCorner);

			//Region regSecondSlice(regTwoSlice);
			//regSecondSlice.setLowerCorner(regSecondSlice.getLowerCorner() + Vector3DInt32(0,0,1));

			Region regSecondSlice(regFirstSlice);
			regSecondSlice.setLowerCorner(regSecondSlice.getLowerCorner() + Vector3DInt32(0,0,1));
			regSecondSlice.setUpperCorner(regSecondSlice.getUpperCorner() + Vector3DInt32(0,0,1));

			boost::uint32_t uNoOfNonEmptyCellsForSlice1 = computeBitmaskForSlice(volIter, regSecondSlice, offset, bitmask1);

			if(uNoOfNonEmptyCellsForSlice1 != 0)
			{
				generateVerticesForSlice(volIter,regSecondSlice, offset, bitmask1, singleMaterialPatch, vertexIndicesX1, vertexIndicesY1, vertexIndicesZ1, /*regTwoSlice.getUpperCorner(),*/ vertlist, vertMaterials);				
			}

			if((uNoOfNonEmptyCellsForSlice0 != 0) || (uNoOfNonEmptyCellsForSlice1 != 0))
			{
				generateExperimentalMeshDataForRegionSlice(volIter, regTwoSlice, singleMaterialPatch, offset, bitmask0, bitmask1, vertexIndicesX0, vertexIndicesY0, vertexIndicesZ0, vertexIndicesX1, vertexIndicesY1, vertexIndicesZ1, vertlist, vertMaterials);
			}

			//memcpy(bitmask0, bitmask1, (POLYVOX_REGION_SIDE_LENGTH+1) * (POLYVOX_REGION_SIDE_LENGTH+1));
			/*boost::uint8_t* temp = bitmask0;
			bitmask0 = bitmask1;
			bitmask1 = temp;*/

			//boost::uint32_t temp = uNoOfNonEmptyCellsForSlice0;
			//uNoOfNonEmptyCellsForSlice0 = uNoOfNonEmptyCellsForSlice1;
			//uNoOfNonEmptyCellsForSlice1 = temp;
			std::swap(uNoOfNonEmptyCellsForSlice0, uNoOfNonEmptyCellsForSlice1);

			std::swap(bitmask0, bitmask1);
			memset(bitmask1, 0, (POLYVOX_REGION_SIDE_LENGTH+1) * (POLYVOX_REGION_SIDE_LENGTH+1));

			//memcpy(vertexIndicesX0, vertexIndicesX1, sizeof(vertexIndicesX0));
			std::swap(vertexIndicesX0, vertexIndicesX1);
			memset(vertexIndicesX1, 0, sizeof(vertexIndicesX1));

			//memcpy(vertexIndicesY0, vertexIndicesY1, sizeof(vertexIndicesY0));
			std::swap(vertexIndicesY0, vertexIndicesY1);
			memset(vertexIndicesY1, 0, sizeof(vertexIndicesY1));

			//memcpy(vertexIndicesZ0, vertexIndicesZ1, sizeof(vertexIndicesZ0));
			std::swap(vertexIndicesZ0, vertexIndicesZ1);
			memset(vertexIndicesZ1, 0, sizeof(vertexIndicesZ1));

			regFirstSlice = regSecondSlice;
		}

		delete[] bitmask0;
		delete[] bitmask1;
		delete[] vertexIndicesX0;
		delete[] vertexIndicesX1;
		delete[] vertexIndicesY0;
		delete[] vertexIndicesY1;
		delete[] vertexIndicesZ0;
		delete[] vertexIndicesZ1;


		std::vector<SurfaceVertex>::iterator iterSurfaceVertex = singleMaterialPatch->getVertices().begin();
		while(iterSurfaceVertex != singleMaterialPatch->getVertices().end())
		{
			Vector3DFloat tempNormal = computeNormal(volumeData, static_cast<Vector3DFloat>(iterSurfaceVertex->getPosition() + offset), CENTRAL_DIFFERENCE);
			const_cast<SurfaceVertex&>(*iterSurfaceVertex).setNormal(tempNormal);
			++iterSurfaceVertex;
		}
	}

	void generateExperimentalMeshDataForRegionSlice(BlockVolumeIterator<uint8_t>& volIter, Region regTwoSlice, IndexedSurfacePatch* singleMaterialPatch, const Vector3DFloat& offset, uint8_t* bitmask0, uint8_t* bitmask1, boost::int32_t vertexIndicesX0[],boost::int32_t vertexIndicesY0[],boost::int32_t vertexIndicesZ0[], boost::int32_t vertexIndicesX1[],boost::int32_t vertexIndicesY1[],boost::int32_t vertexIndicesZ1[], Vector3DFloat vertlist[], uint8_t vertMaterials[])
	{
		Region regFirstSlice(regTwoSlice);
		regFirstSlice.setUpperCorner(regFirstSlice.getUpperCorner() - Vector3DInt32(0,0,1));
		Region regSecondSlice(regTwoSlice);
		regSecondSlice.setLowerCorner(regSecondSlice.getLowerCorner() + Vector3DInt32(0,0,1));


		

		//////////////////////////////////////////////////////////////
		// Set the indices
		//////////////////////////////////////////////////////////////

		boost::uint32_t indlist[12];
		//Iterate over each cell in the region
		regFirstSlice.setUpperCorner(regFirstSlice.getUpperCorner() - Vector3DInt32(1,1,0));
		volIter.setPosition(regFirstSlice.getLowerCorner().getX(),regFirstSlice.getLowerCorner().getY(), regFirstSlice.getLowerCorner().getZ());
		volIter.setValidRegion(regFirstSlice);
		//while(volIter.moveForwardInRegionXYZ())
		do
		{		
			//Current position
			const uint16_t x = volIter.getPosX() - offset.getX();
			const uint16_t y = volIter.getPosY() - offset.getY();
			const uint16_t z = volIter.getPosZ() - offset.getZ();

			//Determine the index into the edge table which tells us which vertices are inside of the surface
			uint8_t iCubeIndex = bitmask0[getIndex(x,y)];

			/* Cube is entirely in/out of the surface */
			if (edgeTable[iCubeIndex] == 0)
			{
				continue;
			}

			/* Find the vertices where the surface intersects the cube */
			if (edgeTable[iCubeIndex] & 1)
			{
				indlist[0] = vertexIndicesX0[getIndex(x,y)];
				assert(indlist[0] != -1);
			}
			if (edgeTable[iCubeIndex] & 2)
			{
				indlist[1] = vertexIndicesY0[getIndex(x+1,y)];
				assert(indlist[1] != -1);
			}
			if (edgeTable[iCubeIndex] & 4)
			{
				indlist[2] = vertexIndicesX0[getIndex(x,y+1)];
				assert(indlist[2] != -1);
			}
			if (edgeTable[iCubeIndex] & 8)
			{
				indlist[3] = vertexIndicesY0[getIndex(x,y)];
				assert(indlist[3] != -1);
			}
			if (edgeTable[iCubeIndex] & 16)
			{
				indlist[4] = vertexIndicesX1[getIndex(x,y)];
				assert(indlist[4] != -1);
			}
			if (edgeTable[iCubeIndex] & 32)
			{
				indlist[5] = vertexIndicesY1[getIndex(x+1,y)];
				assert(indlist[5] != -1);
			}
			if (edgeTable[iCubeIndex] & 64)
			{
				indlist[6] = vertexIndicesX1[getIndex(x,y+1)];
				assert(indlist[6] != -1);
			}
			if (edgeTable[iCubeIndex] & 128)
			{
				indlist[7] = vertexIndicesY1[getIndex(x,y)];
				assert(indlist[7] != -1);
			}
			if (edgeTable[iCubeIndex] & 256)
			{
				indlist[8] = vertexIndicesZ0[getIndex(x,y)];
				assert(indlist[8] != -1);
			}
			if (edgeTable[iCubeIndex] & 512)
			{
				indlist[9] = vertexIndicesZ0[getIndex(x+1,y)];
				assert(indlist[9] != -1);
			}
			if (edgeTable[iCubeIndex] & 1024)
			{
				indlist[10] = vertexIndicesZ0[getIndex(x+1,y+1)];
				assert(indlist[10] != -1);
			}
			if (edgeTable[iCubeIndex] & 2048)
			{
				indlist[11] = vertexIndicesZ0[getIndex(x,y+1)];
				assert(indlist[11] != -1);
			}

			for (int i=0;triTable[iCubeIndex][i]!=-1;i+=3)
			{
				boost::uint32_t ind0 = indlist[triTable[iCubeIndex][i  ]];
				boost::uint32_t ind1 = indlist[triTable[iCubeIndex][i+1]];
				boost::uint32_t ind2 = indlist[triTable[iCubeIndex][i+2]];

				singleMaterialPatch->m_vecTriangleIndices.push_back(ind0);
				singleMaterialPatch->m_vecTriangleIndices.push_back(ind1);
				singleMaterialPatch->m_vecTriangleIndices.push_back(ind2);
			}//For each triangle
		}while(volIter.moveForwardInRegionXYZ());//For each cell
	}

	boost::uint32_t computeBitmaskForSlice(BlockVolumeIterator<uint8_t>& volIter, const Region& regSlice, const Vector3DFloat& offset, uint8_t* bitmask)
	{
		boost::uint32_t uNoOfNonEmptyCells = 0;

		//Iterate over each cell in the region
		volIter.setPosition(regSlice.getLowerCorner().getX(),regSlice.getLowerCorner().getY(), regSlice.getLowerCorner().getZ());
		volIter.setValidRegion(regSlice);
		do
		{		
			//Current position
			const uint16_t x = volIter.getPosX() - offset.getX();
			const uint16_t y = volIter.getPosY() - offset.getY();
			const uint16_t z = volIter.getPosZ() - offset.getZ();

			//Voxels values
			const uint8_t v000 = volIter.getVoxel();
			const uint8_t v100 = volIter.peekVoxel1px0py0pz();
			const uint8_t v010 = volIter.peekVoxel0px1py0pz();
			const uint8_t v110 = volIter.peekVoxel1px1py0pz();
			const uint8_t v001 = volIter.peekVoxel0px0py1pz();
			const uint8_t v101 = volIter.peekVoxel1px0py1pz();
			const uint8_t v011 = volIter.peekVoxel0px1py1pz();
			const uint8_t v111 = volIter.peekVoxel1px1py1pz();

			//Determine the index into the edge table which tells us which vertices are inside of the surface
			uint8_t iCubeIndex = 0;

			if (v000 == 0) iCubeIndex |= 1;
			if (v100 == 0) iCubeIndex |= 2;
			if (v110 == 0) iCubeIndex |= 4;
			if (v010 == 0) iCubeIndex |= 8;
			if (v001 == 0) iCubeIndex |= 16;
			if (v101 == 0) iCubeIndex |= 32;
			if (v111 == 0) iCubeIndex |= 64;
			if (v011 == 0) iCubeIndex |= 128;

			//Save the bitmask
			bitmask[getIndex(x,y)] = iCubeIndex;

			if(edgeTable[iCubeIndex] != 0)
			{
				++uNoOfNonEmptyCells;
			}
			
		}while(volIter.moveForwardInRegionXYZ());//For each cell

		return uNoOfNonEmptyCells;
	}

	void generateVerticesForSlice(BlockVolumeIterator<uint8_t>& volIter, Region& regSlice, const Vector3DFloat& offset, uint8_t* bitmask, IndexedSurfacePatch* singleMaterialPatch,boost::int32_t vertexIndicesX[],boost::int32_t vertexIndicesY[],boost::int32_t vertexIndicesZ[], /*const Vector3DInt32& upperCorner,*/ Vector3DFloat vertlist[], uint8_t vertMaterials[])
	{
		//Iterate over each cell in the region
		volIter.setPosition(regSlice.getLowerCorner().getX(),regSlice.getLowerCorner().getY(), regSlice.getLowerCorner().getZ());
		volIter.setValidRegion(regSlice);
		//while(volIter.moveForwardInRegionXYZ())
		do
		{		
			//Current position
			const uint16_t x = volIter.getPosX() - offset.getX();
			const uint16_t y = volIter.getPosY() - offset.getY();
			const uint16_t z = volIter.getPosZ() - offset.getZ();

			const uint8_t v000 = volIter.getVoxel();

			//Determine the index into the edge table which tells us which vertices are inside of the surface
			uint8_t iCubeIndex = bitmask[getIndex(x,y)];

			/* Cube is entirely in/out of the surface */
			if (edgeTable[iCubeIndex] == 0)
			{
				continue;
			}

			/* Find the vertices where the surface intersects the cube */
			if (edgeTable[iCubeIndex] & 1)
			{
				if((x + offset.getX()) != regSlice.getUpperCorner().getX())
				{
					vertlist[0].setX(x + 0.5f);
					vertlist[0].setY(y);
					vertlist[0].setZ(z);
					vertMaterials[0] = v000 | volIter.peekVoxel1px0py0pz(); //Because one of these is 0, the or operation takes the max.
					SurfaceVertex surfaceVertex(vertlist[0],vertMaterials[0], 1.0);
					singleMaterialPatch->m_vecVertices.push_back(surfaceVertex);
					vertexIndicesX[getIndex(x,y)] = singleMaterialPatch->m_vecVertices.size()-1;
				}
			}
			if (edgeTable[iCubeIndex] & 8)
			{
				if((y + offset.getY()) != regSlice.getUpperCorner().getY())
				{
					vertlist[3].setX(x);
					vertlist[3].setY(y + 0.5f);
					vertlist[3].setZ(z);
					vertMaterials[3] = v000 | volIter.peekVoxel0px1py0pz();
					SurfaceVertex surfaceVertex(vertlist[3],vertMaterials[3], 1.0);
					singleMaterialPatch->m_vecVertices.push_back(surfaceVertex);
					vertexIndicesY[getIndex(x,y)] = singleMaterialPatch->m_vecVertices.size()-1;
				}
			}
			if (edgeTable[iCubeIndex] & 256)
			{
				//if((z + offset.getZ()) != upperCorner.getZ())
				{
					vertlist[8].setX(x);
					vertlist[8].setY(y);
					vertlist[8].setZ(z + 0.5f);
					vertMaterials[8] = v000 | volIter.peekVoxel0px0py1pz();
					SurfaceVertex surfaceVertex(vertlist[8],vertMaterials[8], 1.0);
					singleMaterialPatch->m_vecVertices.push_back(surfaceVertex);
					vertexIndicesZ[getIndex(x,y)] = singleMaterialPatch->m_vecVertices.size()-1;
				}
			}
		}while(volIter.moveForwardInRegionXYZ());//For each cell
	}

	void generateRoughMeshDataForRegion(BlockVolume<uint8_t>* volumeData, Region region, IndexedSurfacePatch* singleMaterialPatch)
	{	
		//When generating the mesh for a region we actually look one voxel outside it in the
		// back, bottom, right direction. Protect against access violations by cropping region here
		Region regVolume = volumeData->getEnclosingRegion();
		//regVolume.setUpperCorner(regVolume.getUpperCorner() - Vector3DInt32(1,1,1));
		region.cropTo(regVolume);
		region.setUpperCorner(region.getUpperCorner() - Vector3DInt32(1,1,1));

		//Offset from lower block corner
		const Vector3DFloat offset = static_cast<Vector3DFloat>(region.getLowerCorner());

		Vector3DFloat vertlist[12];
		uint8_t vertMaterials[12];
		BlockVolumeIterator<boost::uint8_t> volIter(*volumeData);
		volIter.setValidRegion(region);

		//////////////////////////////////////////////////////////////////////////
		//Get mesh data
		//////////////////////////////////////////////////////////////////////////

		//Iterate over each cell in the region
		volIter.setPosition(region.getLowerCorner().getX(),region.getLowerCorner().getY(), region.getLowerCorner().getZ());
		while(volIter.moveForwardInRegionXYZ())
		{		
			//Current position
			const uint16_t x = volIter.getPosX();
			const uint16_t y = volIter.getPosY();
			const uint16_t z = volIter.getPosZ();

			//Voxels values
			const uint8_t v000 = volIter.getVoxel();
			const uint8_t v100 = volIter.peekVoxel1px0py0pz();
			const uint8_t v010 = volIter.peekVoxel0px1py0pz();
			const uint8_t v110 = volIter.peekVoxel1px1py0pz();
			const uint8_t v001 = volIter.peekVoxel0px0py1pz();
			const uint8_t v101 = volIter.peekVoxel1px0py1pz();
			const uint8_t v011 = volIter.peekVoxel0px1py1pz();
			const uint8_t v111 = volIter.peekVoxel1px1py1pz();

			//Determine the index into the edge table which tells us which vertices are inside of the surface
			uint8_t iCubeIndex = 0;

			if (v000 == 0) iCubeIndex |= 1;
			if (v100 == 0) iCubeIndex |= 2;
			if (v110 == 0) iCubeIndex |= 4;
			if (v010 == 0) iCubeIndex |= 8;
			if (v001 == 0) iCubeIndex |= 16;
			if (v101 == 0) iCubeIndex |= 32;
			if (v111 == 0) iCubeIndex |= 64;
			if (v011 == 0) iCubeIndex |= 128;

			/* Cube is entirely in/out of the surface */
			if (edgeTable[iCubeIndex] == 0)
			{
				continue;
			}

			/* Find the vertices where the surface intersects the cube */
			if (edgeTable[iCubeIndex] & 1)
			{
				vertlist[0].setX(x + 0.5f);
				vertlist[0].setY(y);
				vertlist[0].setZ(z);
				vertMaterials[0] = v000 | v100; //Because one of these is 0, the or operation takes the max.
			}
			if (edgeTable[iCubeIndex] & 2)
			{
				vertlist[1].setX(x + 1.0f);
				vertlist[1].setY(y + 0.5f);
				vertlist[1].setZ(z);
				vertMaterials[1] = v100 | v110;
			}
			if (edgeTable[iCubeIndex] & 4)
			{
				vertlist[2].setX(x + 0.5f);
				vertlist[2].setY(y + 1.0f);
				vertlist[2].setZ(z);
				vertMaterials[2] = v010 | v110;
			}
			if (edgeTable[iCubeIndex] & 8)
			{
				vertlist[3].setX(x);
				vertlist[3].setY(y + 0.5f);
				vertlist[3].setZ(z);
				vertMaterials[3] = v000 | v010;
			}
			if (edgeTable[iCubeIndex] & 16)
			{
				vertlist[4].setX(x + 0.5f);
				vertlist[4].setY(y);
				vertlist[4].setZ(z + 1.0f);
				vertMaterials[4] = v001 | v101;
			}
			if (edgeTable[iCubeIndex] & 32)
			{
				vertlist[5].setX(x + 1.0f);
				vertlist[5].setY(y + 0.5f);
				vertlist[5].setZ(z + 1.0f);
				vertMaterials[5] = v101 | v111;
			}
			if (edgeTable[iCubeIndex] & 64)
			{
				vertlist[6].setX(x + 0.5f);
				vertlist[6].setY(y + 1.0f);
				vertlist[6].setZ(z + 1.0f);
				vertMaterials[6] = v011 | v111;
			}
			if (edgeTable[iCubeIndex] & 128)
			{
				vertlist[7].setX(x);
				vertlist[7].setY(y + 0.5f);
				vertlist[7].setZ(z + 1.0f);
				vertMaterials[7] = v001 | v011;
			}
			if (edgeTable[iCubeIndex] & 256)
			{
				vertlist[8].setX(x);
				vertlist[8].setY(y);
				vertlist[8].setZ(z + 0.5f);
				vertMaterials[8] = v000 | v001;
			}
			if (edgeTable[iCubeIndex] & 512)
			{
				vertlist[9].setX(x + 1.0f);
				vertlist[9].setY(y);
				vertlist[9].setZ(z + 0.5f);
				vertMaterials[9] = v100 | v101;
			}
			if (edgeTable[iCubeIndex] & 1024)
			{
				vertlist[10].setX(x + 1.0f);
				vertlist[10].setY(y + 1.0f);
				vertlist[10].setZ(z + 0.5f);
				vertMaterials[10] = v110 | v111;
			}
			if (edgeTable[iCubeIndex] & 2048)
			{
				vertlist[11].setX(x);
				vertlist[11].setY(y + 1.0f);
				vertlist[11].setZ(z + 0.5f);
				vertMaterials[11] = v010 | v011;
			}

			for (int i=0;triTable[iCubeIndex][i]!=-1;i+=3)
			{
				//The three vertices forming a triangle
				const Vector3DFloat vertex0 = vertlist[triTable[iCubeIndex][i  ]] - offset;
				const Vector3DFloat vertex1 = vertlist[triTable[iCubeIndex][i+1]] - offset;
				const Vector3DFloat vertex2 = vertlist[triTable[iCubeIndex][i+2]] - offset;

				//Cast to floats and divide by two.
				//const Vector3DFloat vertex0AsFloat = (static_cast<Vector3DFloat>(vertex0) / 2.0f) - offset;
				//const Vector3DFloat vertex1AsFloat = (static_cast<Vector3DFloat>(vertex1) / 2.0f) - offset;
				//const Vector3DFloat vertex2AsFloat = (static_cast<Vector3DFloat>(vertex2) / 2.0f) - offset;

				const uint8_t material0 = vertMaterials[triTable[iCubeIndex][i  ]];
				const uint8_t material1 = vertMaterials[triTable[iCubeIndex][i+1]];
				const uint8_t material2 = vertMaterials[triTable[iCubeIndex][i+2]];


				//If all the materials are the same, we just need one triangle for that material with all the alphas set high.
				SurfaceVertex surfaceVertex0Alpha1(vertex0,material0 + 0.1f,1.0f);
				SurfaceVertex surfaceVertex1Alpha1(vertex1,material1 + 0.1f,1.0f);
				SurfaceVertex surfaceVertex2Alpha1(vertex2,material2 + 0.1f,1.0f);
				singleMaterialPatch->addTriangle(surfaceVertex0Alpha1, surfaceVertex1Alpha1, surfaceVertex2Alpha1);
			}//For each triangle
		}//For each cell

		//FIXME - can it happen that we have no vertices or triangles? Should exit early?


		//for(std::map<uint8_t, IndexedSurfacePatch*>::iterator iterPatch = surfacePatchMapResult.begin(); iterPatch != surfacePatchMapResult.end(); ++iterPatch)
		{

			std::vector<SurfaceVertex>::iterator iterSurfaceVertex = singleMaterialPatch->getVertices().begin();
			while(iterSurfaceVertex != singleMaterialPatch->getVertices().end())
			{
				Vector3DFloat tempNormal = computeNormal(volumeData, static_cast<Vector3DFloat>(iterSurfaceVertex->getPosition() + offset), CENTRAL_DIFFERENCE);
				const_cast<SurfaceVertex&>(*iterSurfaceVertex).setNormal(tempNormal);
				++iterSurfaceVertex;
			}
		}
	}

	Vector3DFloat computeNormal(BlockVolume<uint8_t>* volumeData, const Vector3DFloat& position, NormalGenerationMethod normalGenerationMethod)
	{
		const float posX = position.getX();
		const float posY = position.getY();
		const float posZ = position.getZ();

		const uint16_t floorX = static_cast<uint16_t>(posX);
		const uint16_t floorY = static_cast<uint16_t>(posY);
		const uint16_t floorZ = static_cast<uint16_t>(posZ);

		//Check all corners are within the volume, allowing a boundary for gradient estimation
		bool lowerCornerInside = volumeData->containsPoint(Vector3DInt32(floorX, floorY, floorZ),1);
		bool upperCornerInside = volumeData->containsPoint(Vector3DInt32(floorX+1, floorY+1, floorZ+1),1);
		if((!lowerCornerInside) || (!upperCornerInside))
		{
			normalGenerationMethod = SIMPLE;
		}

		Vector3DFloat result;

		BlockVolumeIterator<boost::uint8_t> volIter(*volumeData); //FIXME - save this somewhere - could be expensive to create?


		if(normalGenerationMethod == SOBEL)
		{
			volIter.setPosition(static_cast<uint16_t>(posX),static_cast<uint16_t>(posY),static_cast<uint16_t>(posZ));
			const Vector3DFloat gradFloor = computeSobelGradient(volIter);
			if((posX - floorX) > 0.25) //The result should be 0.0 or 0.5
			{			
				volIter.setPosition(static_cast<uint16_t>(posX+1.0),static_cast<uint16_t>(posY),static_cast<uint16_t>(posZ));
			}
			if((posY - floorY) > 0.25) //The result should be 0.0 or 0.5
			{			
				volIter.setPosition(static_cast<uint16_t>(posX),static_cast<uint16_t>(posY+1.0),static_cast<uint16_t>(posZ));
			}
			if((posZ - floorZ) > 0.25) //The result should be 0.0 or 0.5
			{			
				volIter.setPosition(static_cast<uint16_t>(posX),static_cast<uint16_t>(posY),static_cast<uint16_t>(posZ+1.0));					
			}
			const Vector3DFloat gradCeil = computeSobelGradient(volIter);
			result = ((gradFloor + gradCeil) * -1.0f);
			if(result.lengthSquared() < 0.0001)
			{
				//Operation failed - fall back on simple gradient estimation
				normalGenerationMethod = SIMPLE;
			}
		}
		if(normalGenerationMethod == CENTRAL_DIFFERENCE)
		{
			volIter.setPosition(static_cast<uint16_t>(posX),static_cast<uint16_t>(posY),static_cast<uint16_t>(posZ));
			const Vector3DFloat gradFloor = computeCentralDifferenceGradient(volIter);
			if((posX - floorX) > 0.25) //The result should be 0.0 or 0.5
			{			
				volIter.setPosition(static_cast<uint16_t>(posX+1.0),static_cast<uint16_t>(posY),static_cast<uint16_t>(posZ));
			}
			if((posY - floorY) > 0.25) //The result should be 0.0 or 0.5
			{			
				volIter.setPosition(static_cast<uint16_t>(posX),static_cast<uint16_t>(posY+1.0),static_cast<uint16_t>(posZ));
			}
			if((posZ - floorZ) > 0.25) //The result should be 0.0 or 0.5
			{			
				volIter.setPosition(static_cast<uint16_t>(posX),static_cast<uint16_t>(posY),static_cast<uint16_t>(posZ+1.0));					
			}
			const Vector3DFloat gradCeil = computeCentralDifferenceGradient(volIter);
			result = ((gradFloor + gradCeil) * -1.0f);
			if(result.lengthSquared() < 0.0001)
			{
				//Operation failed - fall back on simple gradient estimation
				normalGenerationMethod = SIMPLE;
			}
		}
		if(normalGenerationMethod == SIMPLE)
		{
			volIter.setPosition(static_cast<uint16_t>(posX),static_cast<uint16_t>(posY),static_cast<uint16_t>(posZ));
			const uint8_t uFloor = volIter.getVoxel() > 0 ? 1 : 0;
			if((posX - floorX) > 0.25) //The result should be 0.0 or 0.5
			{					
				uint8_t uCeil = volIter.peekVoxel1px0py0pz() > 0 ? 1 : 0;
				result = Vector3DFloat(static_cast<float>(uFloor - uCeil),0.0,0.0);
			}
			else if((posY - floorY) > 0.25) //The result should be 0.0 or 0.5
			{
				uint8_t uCeil = volIter.peekVoxel0px1py0pz() > 0 ? 1 : 0;
				result = Vector3DFloat(0.0,static_cast<float>(uFloor - uCeil),0.0);
			}
			else if((posZ - floorZ) > 0.25) //The result should be 0.0 or 0.5
			{
				uint8_t uCeil = volIter.peekVoxel0px0py1pz() > 0 ? 1 : 0;
				result = Vector3DFloat(0.0, 0.0,static_cast<float>(uFloor - uCeil));					
			}
		}
		return result;
	}

	void generateSmoothMeshDataForRegion(BlockVolume<uint8_t>* volumeData, Region region, IndexedSurfacePatch* singleMaterialPatch)
	{	
		//When generating the mesh for a region we actually look one voxel outside it in the
		// back, bottom, right direction. Protect against access violations by cropping region here
		Region regVolume = volumeData->getEnclosingRegion();
		regVolume.setUpperCorner(regVolume.getUpperCorner() - Vector3DInt32(1,1,1));
		region.cropTo(regVolume);

		//Offset from lower block corner
		const Vector3DFloat offset = static_cast<Vector3DFloat>(region.getLowerCorner());

		Vector3DFloat vertlist[12];
		uint8_t vertMaterials[12];
		BlockVolumeIterator<boost::uint8_t> volIter(*volumeData);
		volIter.setValidRegion(region);

		const float threshold = 0.5f;

		//////////////////////////////////////////////////////////////////////////
		//Get mesh data
		//////////////////////////////////////////////////////////////////////////

		//Iterate over each cell in the region
		for(volIter.setPosition(region.getLowerCorner().getX(),region.getLowerCorner().getY(), region.getLowerCorner().getZ());volIter.isValidForRegion();volIter.moveForwardInRegionXYZ())
		{		
			//Current position
			const uint16_t x = volIter.getPosX();
			const uint16_t y = volIter.getPosY();
			const uint16_t z = volIter.getPosZ();

			//Voxels values
			BlockVolumeIterator<boost::uint8_t> tempVolIter(*volumeData);
			tempVolIter.setPosition(x,y,z);
			const float v000 = tempVolIter.getAveragedVoxel(1);
			tempVolIter.setPosition(x+1,y,z);
			const float v100 = tempVolIter.getAveragedVoxel(1);
			tempVolIter.setPosition(x,y+1,z);
			const float v010 = tempVolIter.getAveragedVoxel(1);
			tempVolIter.setPosition(x+1,y+1,z);
			const float v110 = tempVolIter.getAveragedVoxel(1);
			tempVolIter.setPosition(x,y,z+1);
			const float v001 = tempVolIter.getAveragedVoxel(1);
			tempVolIter.setPosition(x+1,y,z+1);
			const float v101 = tempVolIter.getAveragedVoxel(1);
			tempVolIter.setPosition(x,y+1,z+1);
			const float v011 = tempVolIter.getAveragedVoxel(1);
			tempVolIter.setPosition(x+1,y+1,z+1);
			const float v111 = tempVolIter.getAveragedVoxel(1);

			//Determine the index into the edge table which tells us which vertices are inside of the surface
			uint8_t iCubeIndex = 0;

			if (v000 < threshold) iCubeIndex |= 1;
			if (v100 < threshold) iCubeIndex |= 2;
			if (v110 < threshold) iCubeIndex |= 4;
			if (v010 < threshold) iCubeIndex |= 8;
			if (v001 < threshold) iCubeIndex |= 16;
			if (v101 < threshold) iCubeIndex |= 32;
			if (v111 < threshold) iCubeIndex |= 64;
			if (v011 < threshold) iCubeIndex |= 128;

			/* Cube is entirely in/out of the surface */
			if (edgeTable[iCubeIndex] == 0)
			{
				continue;
			}

			/* Find the vertices where the surface intersects the cube */
			if (edgeTable[iCubeIndex] & 1)
			{
				float a = v000;
				float b = v100;
				float val = (threshold-a)/(b-a);
				vertlist[0].setX(x + val);
				vertlist[0].setY(y);
				vertlist[0].setZ(z);
				vertMaterials[0] = 1;//v000 | v100; //Because one of these is 0, the or operation takes the max.
			}
			if (edgeTable[iCubeIndex] & 2)
			{
				float a = v100;
				float b = v110;
				float val = (threshold-a)/(b-a);
				vertlist[1].setX(x + 1.0f);
				vertlist[1].setY(y + val);
				vertlist[1].setZ(z);
				vertMaterials[1] = 1;//v100 | v110;
			}
			if (edgeTable[iCubeIndex] & 4)
			{
				float a = v010;
				float b = v110;
				float val = (threshold-a)/(b-a);
				vertlist[2].setX(x + val);
				vertlist[2].setY(y + 1.0f);
				vertlist[2].setZ(z);
				vertMaterials[2] = 1;//v010 | v110;
			}
			if (edgeTable[iCubeIndex] & 8)
			{
				float a = v000;
				float b = v010;
				float val = (threshold-a)/(b-a);
				vertlist[3].setX(x);
				vertlist[3].setY(y + val);
				vertlist[3].setZ(z);
				vertMaterials[3] = 1;//v000 | v010;
			}
			if (edgeTable[iCubeIndex] & 16)
			{
				float a = v001;
				float b = v101;
				float val = (threshold-a)/(b-a);
				vertlist[4].setX(x + val);
				vertlist[4].setY(y);
				vertlist[4].setZ(z + 1.0f);
				vertMaterials[4] = 1;//v001 | v101;
			}
			if (edgeTable[iCubeIndex] & 32)
			{
				float a = v101;
				float b = v111;
				float val = (threshold-a)/(b-a);
				vertlist[5].setX(x + 1.0f);
				vertlist[5].setY(y + val);
				vertlist[5].setZ(z + 1.0f);
				vertMaterials[5] = 1;//v101 | v111;
			}
			if (edgeTable[iCubeIndex] & 64)
			{
				float a = v011;
				float b = v111;
				float val = (threshold-a)/(b-a);
				vertlist[6].setX(x + val);
				vertlist[6].setY(y + 1.0f);
				vertlist[6].setZ(z + 1.0f);
				vertMaterials[6] = 1;//v011 | v111;
			}
			if (edgeTable[iCubeIndex] & 128)
			{
				float a = v001;
				float b = v011;
				float val = (threshold-a)/(b-a);
				vertlist[7].setX(x);
				vertlist[7].setY(y + val);
				vertlist[7].setZ(z + 1.0f);
				vertMaterials[7] = 1;//v001 | v011;
			}
			if (edgeTable[iCubeIndex] & 256)
			{
				float a = v000;
				float b = v001;
				float val = (threshold-a)/(b-a);
				vertlist[8].setX(x);
				vertlist[8].setY(y);
				vertlist[8].setZ(z + val);
				vertMaterials[8] = 1;//v000 | v001;
			}
			if (edgeTable[iCubeIndex] & 512)
			{
				float a = v100;
				float b = v101;
				float val = (threshold-a)/(b-a);
				vertlist[9].setX(x + 1.0f);
				vertlist[9].setY(y);
				vertlist[9].setZ(z + val);
				vertMaterials[9] = 1;//v100 | v101;
			}
			if (edgeTable[iCubeIndex] & 1024)
			{
				float a = v110;
				float b = v111;
				float val = (threshold-a)/(b-a);
				vertlist[10].setX(x + 1.0f);
				vertlist[10].setY(y + 1.0f);
				vertlist[10].setZ(z + val);
				vertMaterials[10] = 1;//v110 | v111;
			}
			if (edgeTable[iCubeIndex] & 2048)
			{
				float a = v010;
				float b = v011;
				float val = (threshold-a)/(b-a);
				vertlist[11].setX(x);
				vertlist[11].setY(y + 1.0f);
				vertlist[11].setZ(z + val);
				vertMaterials[11] = 1;//v010 | v011;
			}

			for (int i=0;triTable[iCubeIndex][i]!=-1;i+=3)
			{
				//The three vertices forming a triangle
				const Vector3DFloat vertex0 = vertlist[triTable[iCubeIndex][i  ]] - offset;
				const Vector3DFloat vertex1 = vertlist[triTable[iCubeIndex][i+1]] - offset;
				const Vector3DFloat vertex2 = vertlist[triTable[iCubeIndex][i+2]] - offset;

				const uint8_t material0 = vertMaterials[triTable[iCubeIndex][i  ]];
				const uint8_t material1 = vertMaterials[triTable[iCubeIndex][i+1]];
				const uint8_t material2 = vertMaterials[triTable[iCubeIndex][i+2]];

				SurfaceVertex surfaceVertex0Alpha1(vertex0,material0 + 0.1f,1.0f);
				SurfaceVertex surfaceVertex1Alpha1(vertex1,material1 + 0.1f,1.0f);
				SurfaceVertex surfaceVertex2Alpha1(vertex2,material2 + 0.1f,1.0f);
				singleMaterialPatch->addTriangle(surfaceVertex0Alpha1, surfaceVertex1Alpha1, surfaceVertex2Alpha1);
					
			}//For each triangle
		}//For each cell

		//FIXME - can it happen that we have no vertices or triangles? Should exit early?


		//for(std::map<uint8_t, IndexedSurfacePatch*>::iterator iterPatch = surfacePatchMapResult.begin(); iterPatch != surfacePatchMapResult.end(); ++iterPatch)
		{

			std::vector<SurfaceVertex>::iterator iterSurfaceVertex = singleMaterialPatch->getVertices().begin();
			while(iterSurfaceVertex != singleMaterialPatch->getVertices().end())
			{
				Vector3DFloat tempNormal = computeSmoothNormal(volumeData, static_cast<Vector3DFloat>(iterSurfaceVertex->getPosition() + offset), CENTRAL_DIFFERENCE);
				const_cast<SurfaceVertex&>(*iterSurfaceVertex).setNormal(tempNormal);
				++iterSurfaceVertex;
			}
		}
	}

	Vector3DFloat computeSmoothNormal(BlockVolume<uint8_t>* volumeData, const Vector3DFloat& position, NormalGenerationMethod normalGenerationMethod)
	{
		

		const float posX = position.getX();
		const float posY = position.getY();
		const float posZ = position.getZ();

		const uint16_t floorX = static_cast<uint16_t>(posX);
		const uint16_t floorY = static_cast<uint16_t>(posY);
		const uint16_t floorZ = static_cast<uint16_t>(posZ);

		//Check all corners are within the volume, allowing a boundary for gradient estimation
		bool lowerCornerInside = volumeData->containsPoint(Vector3DInt32(floorX, floorY, floorZ),1);
		bool upperCornerInside = volumeData->containsPoint(Vector3DInt32(floorX+1, floorY+1, floorZ+1),1);
		if((!lowerCornerInside) || (!upperCornerInside))
		{
			normalGenerationMethod = SIMPLE;
		}

		Vector3DFloat result;

		BlockVolumeIterator<boost::uint8_t> volIter(*volumeData); //FIXME - save this somewhere - could be expensive to create?


		if(normalGenerationMethod == SOBEL)
		{
			volIter.setPosition(static_cast<uint16_t>(posX),static_cast<uint16_t>(posY),static_cast<uint16_t>(posZ));
			const Vector3DFloat gradFloor = computeSobelGradient(volIter);
			if((posX - floorX) > 0.25) //The result should be 0.0 or 0.5
			{			
				volIter.setPosition(static_cast<uint16_t>(posX+1.0),static_cast<uint16_t>(posY),static_cast<uint16_t>(posZ));
			}
			if((posY - floorY) > 0.25) //The result should be 0.0 or 0.5
			{			
				volIter.setPosition(static_cast<uint16_t>(posX),static_cast<uint16_t>(posY+1.0),static_cast<uint16_t>(posZ));
			}
			if((posZ - floorZ) > 0.25) //The result should be 0.0 or 0.5
			{			
				volIter.setPosition(static_cast<uint16_t>(posX),static_cast<uint16_t>(posY),static_cast<uint16_t>(posZ+1.0));					
			}
			const Vector3DFloat gradCeil = computeSobelGradient(volIter);
			result = ((gradFloor + gradCeil) * -1.0f);
			if(result.lengthSquared() < 0.0001)
			{
				//Operation failed - fall back on simple gradient estimation
				normalGenerationMethod = SIMPLE;
			}
		}
		if(normalGenerationMethod == CENTRAL_DIFFERENCE)
		{
			volIter.setPosition(static_cast<uint16_t>(posX),static_cast<uint16_t>(posY),static_cast<uint16_t>(posZ));
			const Vector3DFloat gradFloor = computeSmoothCentralDifferenceGradient(volIter);
			if((posX - floorX) > 0.25) //The result should be 0.0 or 0.5
			{			
				volIter.setPosition(static_cast<uint16_t>(posX+1.0),static_cast<uint16_t>(posY),static_cast<uint16_t>(posZ));
			}
			if((posY - floorY) > 0.25) //The result should be 0.0 or 0.5
			{			
				volIter.setPosition(static_cast<uint16_t>(posX),static_cast<uint16_t>(posY+1.0),static_cast<uint16_t>(posZ));
			}
			if((posZ - floorZ) > 0.25) //The result should be 0.0 or 0.5
			{			
				volIter.setPosition(static_cast<uint16_t>(posX),static_cast<uint16_t>(posY),static_cast<uint16_t>(posZ+1.0));					
			}
			const Vector3DFloat gradCeil = computeSmoothCentralDifferenceGradient(volIter);
			result = ((gradFloor + gradCeil) * -1.0f);
			if(result.lengthSquared() < 0.0001)
			{
				//Operation failed - fall back on simple gradient estimation
				normalGenerationMethod = SIMPLE;
			}
		}
		if(normalGenerationMethod == SIMPLE)
		{
			volIter.setPosition(static_cast<uint16_t>(posX),static_cast<uint16_t>(posY),static_cast<uint16_t>(posZ));
			const uint8_t uFloor = volIter.getVoxel() > 0 ? 1 : 0;
			if((posX - floorX) > 0.25) //The result should be 0.0 or 0.5
			{					
				uint8_t uCeil = volIter.peekVoxel1px0py0pz() > 0 ? 1 : 0;
				result = Vector3DFloat(static_cast<float>(uFloor - uCeil),0.0,0.0);
			}
			else if((posY - floorY) > 0.25) //The result should be 0.0 or 0.5
			{
				uint8_t uCeil = volIter.peekVoxel0px1py0pz() > 0 ? 1 : 0;
				result = Vector3DFloat(0.0,static_cast<float>(uFloor - uCeil),0.0);
			}
			else if((posZ - floorZ) > 0.25) //The result should be 0.0 or 0.5
			{
				uint8_t uCeil = volIter.peekVoxel0px0py1pz() > 0 ? 1 : 0;
				result = Vector3DFloat(0.0, 0.0,static_cast<float>(uFloor - uCeil));					
			}
		}
		return result;
	}
}
