#include "SurfaceExtractor.h"

#include "IndexedSurfacePatch.h"
#include "PolyVoxImpl/MarchingCubesTables.h"
#include "SurfaceVertex.h"

namespace PolyVox
{
	SurfaceExtractor::SurfaceExtractor(Volume<uint8_t>& volData)
		:m_uLodLevel(0)
		,m_volData(volData)
		,m_iterVolume(volData)
	{
	}

	uint8_t SurfaceExtractor::getLodLevel(void)
	{
		return m_uLodLevel;
	}

	void SurfaceExtractor::setLodLevel(uint8_t uLodLevel)
	{
		m_uLodLevel = uLodLevel;
	}

	POLYVOX_SHARED_PTR<IndexedSurfacePatch> SurfaceExtractor::extractSurfaceForRegion(Region region)
	{
		POLYVOX_SHARED_PTR<IndexedSurfacePatch> result(new IndexedSurfacePatch());

		if(m_uLodLevel == 0)
		{
			extractSurfaceForRegionLevel0(&m_volData, region, result.get());
		}
		else
		{
			extractDecimatedSurfaceImpl(&m_volData, m_uLodLevel, region, result.get());
		}
		result->m_Region = region;

		return result;
	}

	uint32_t SurfaceExtractor::getIndex(uint32_t x, uint32_t y, uint32_t regionWidth)
	{
		return x + (y * (regionWidth+1));
	}

	////////////////////////////////////////////////////////////////////////////////
	// Level 0
	////////////////////////////////////////////////////////////////////////////////

	void SurfaceExtractor::extractSurfaceForRegionLevel0(Volume<uint8_t>* volumeData, Region region, IndexedSurfacePatch* singleMaterialPatch)
	{	
		singleMaterialPatch->clear();

		//For edge indices
		int32_t* vertexIndicesX0 = new int32_t[(region.width()+8) * (region.height()+8)];
		int32_t* vertexIndicesY0 = new int32_t[(region.width()+8) * (region.height()+8)];
		int32_t* vertexIndicesZ0 = new int32_t[(region.width()+8) * (region.height()+8)];
		int32_t* vertexIndicesX1 = new int32_t[(region.width()+8) * (region.height()+8)];
		int32_t* vertexIndicesY1 = new int32_t[(region.width()+8) * (region.height()+8)];
		int32_t* vertexIndicesZ1 = new int32_t[(region.width()+8) * (region.height()+8)];

		//Cell bitmasks
		uint8_t* bitmask0 = new uint8_t[(region.width()+8) * (region.height()+8)];
		uint8_t* bitmask1 = new uint8_t[(region.width()+8) * (region.height()+8)];

		//When generating the mesh for a region we actually look one voxel outside it in the
		// back, bottom, right direction. Protect against access violations by cropping region here
		Region regVolume = volumeData->getEnclosingRegion();
		//regVolume.setUpperCorner(regVolume.getUpperCorner() - Vector3DInt32(1,1,1));
		region.cropTo(regVolume);

		//Offset from volume corner
		const Vector3DFloat offset = static_cast<Vector3DFloat>(region.getLowerCorner());

		//Create a region corresponding to the first slice
		Region regSlice0(region);
		regSlice0.setUpperCorner(Vector3DInt32(regSlice0.getUpperCorner().getX(),regSlice0.getUpperCorner().getY(),regSlice0.getLowerCorner().getZ()));

		//Iterator to access the volume data
		VolumeSampler<uint8_t> volIter(*volumeData);		

		//Compute bitmask for initial slice
		uint32_t uNoOfNonEmptyCellsForSlice0 = computeBitmaskForSlice(volIter, 0, regSlice0, offset, bitmask0, 0);		
		if(uNoOfNonEmptyCellsForSlice0 != 0)
		{
			//If there were some non-empty cells then generate initial slice vertices for them
			generateVerticesForSlice(volIter, 0, regSlice0, offset, bitmask0, singleMaterialPatch, vertexIndicesX0, vertexIndicesY0, vertexIndicesZ0);
		}

		for(uint32_t uSlice = 0; ((uSlice < region.depth()) && (uSlice + offset.getZ() < region.getUpperCorner().getZ())); ++uSlice)
		{
			Region regSlice1(regSlice0);
			regSlice1.shift(Vector3DInt32(0,0,1));

			uint32_t uNoOfNonEmptyCellsForSlice1 = computeBitmaskForSlice(volIter, 0, regSlice1, offset, bitmask1, bitmask0);

			if(uNoOfNonEmptyCellsForSlice1 != 0)
			{
				generateVerticesForSlice(volIter, 0, regSlice1, offset, bitmask1, singleMaterialPatch, vertexIndicesX1, vertexIndicesY1, vertexIndicesZ1);				
			}

			if((uNoOfNonEmptyCellsForSlice0 != 0) || (uNoOfNonEmptyCellsForSlice1 != 0))
			{
				generateIndicesForSlice(volIter, 0, regSlice0, singleMaterialPatch, offset, bitmask0, bitmask1, vertexIndicesX0, vertexIndicesY0, vertexIndicesZ0, vertexIndicesX1, vertexIndicesY1, vertexIndicesZ1);
			}

			std::swap(uNoOfNonEmptyCellsForSlice0, uNoOfNonEmptyCellsForSlice1);
			std::swap(bitmask0, bitmask1);
			std::swap(vertexIndicesX0, vertexIndicesX1);
			std::swap(vertexIndicesY0, vertexIndicesY1);
			std::swap(vertexIndicesZ0, vertexIndicesZ1);

			regSlice0 = regSlice1;
		}

		delete[] bitmask0;
		delete[] bitmask1;
		delete[] vertexIndicesX0;
		delete[] vertexIndicesX1;
		delete[] vertexIndicesY0;
		delete[] vertexIndicesY1;
		delete[] vertexIndicesZ0;
		delete[] vertexIndicesZ1;
	}

	/*uint32_t SurfaceExtractor::computeBitmaskForSliceLevel0(VolumeSampler<uint8_t>& volIter, const Region& regSlice, const Vector3DFloat& offset, uint8_t* bitmask, uint8_t* previousBitmask)
	{
		uint32_t uNoOfNonEmptyCells = 0;

		//Iterate over each cell in the region
		for(uint16_t uYVolSpace = regSlice.getLowerCorner().getY(); uYVolSpace <= regSlice.getUpperCorner().getY(); uYVolSpace++)
		{
			for(uint16_t uXVolSpace = regSlice.getLowerCorner().getX(); uXVolSpace <= regSlice.getUpperCorner().getX(); uXVolSpace++)
			{		
				uint16_t uZVolSpace = regSlice.getLowerCorner().getZ();
				volIter.setPosition(uXVolSpace,uYVolSpace,uZVolSpace);
				//Current position
				const uint16_t uXRegSpace = volIter.getPosX() - offset.getX();
				const uint16_t uYRegSpace = volIter.getPosY() - offset.getY();

				//Determine the index into the edge table which tells us which vertices are inside of the surface
				uint8_t iCubeIndex = 0;

				if((uXVolSpace < volIter.getVolume().getWidth()-1) &&
					(uYVolSpace < volIter.getVolume().getHeight()-1) &&
					(uZVolSpace < volIter.getVolume().getDepth()-1))
				{
					bool isPrevXAvail = uXRegSpace > 0;
					bool isPrevYAvail = uYRegSpace > 0;
					bool isPrevZAvail = previousBitmask != 0;

					if(isPrevZAvail)
					{
						if(isPrevYAvail)
						{
							if(isPrevXAvail)
							{
								const uint8_t v111 = volIter.peekVoxel1px1py1pz();			

								//z
								uint8_t iPreviousCubeIndexZ = previousBitmask[getIndex(uXRegSpace,uYRegSpace, regSlice.width()+1)];
								iPreviousCubeIndexZ >>= 4;

								//y
								uint8_t iPreviousCubeIndexY = bitmask[getIndex(uXRegSpace,uYRegSpace-1, regSlice.width()+1)];
								iPreviousCubeIndexY &= 204; //204 = 128+64+8+4
								iPreviousCubeIndexY >>= 2;

								//x
								uint8_t iPreviousCubeIndexX = bitmask[getIndex(uXRegSpace-1,uYRegSpace, regSlice.width()+1)];
								iPreviousCubeIndexX &= 170; //170 = 128+32+8+2
								iPreviousCubeIndexX >>= 1;

								iCubeIndex = iPreviousCubeIndexX | iPreviousCubeIndexY | iPreviousCubeIndexZ;

								if (v111 == 0) iCubeIndex |= 128;
							}
							else //previous X not available
							{
								const uint8_t v011 = volIter.peekVoxel0px1py1pz();
								const uint8_t v111 = volIter.peekVoxel1px1py1pz();

								//z
								uint8_t iPreviousCubeIndexZ = previousBitmask[getIndex(uXRegSpace,uYRegSpace, regSlice.width()+1)];
								iPreviousCubeIndexZ >>= 4;

								//y
								uint8_t iPreviousCubeIndexY = bitmask[getIndex(uXRegSpace,uYRegSpace-1, regSlice.width()+1)];
								iPreviousCubeIndexY &= 192; //192 = 128 + 64
								iPreviousCubeIndexY >>= 2;

								iCubeIndex = iPreviousCubeIndexY | iPreviousCubeIndexZ;

								if (v011 == 0) iCubeIndex |= 64;
								if (v111 == 0) iCubeIndex |= 128;
							}
						}
						else //previous Y not available
						{
							if(isPrevXAvail)
							{
								const uint8_t v101 = volIter.peekVoxel1px0py1pz();
								const uint8_t v111 = volIter.peekVoxel1px1py1pz();			

								//z
								uint8_t iPreviousCubeIndexZ = previousBitmask[getIndex(uXRegSpace,uYRegSpace, regSlice.width()+1)];
								iPreviousCubeIndexZ >>= 4;

								//x
								uint8_t iPreviousCubeIndexX = bitmask[getIndex(uXRegSpace-1,uYRegSpace, regSlice.width()+1)];
								iPreviousCubeIndexX &= 160; //160 = 128+32
								iPreviousCubeIndexX >>= 1;

								iCubeIndex = iPreviousCubeIndexX | iPreviousCubeIndexZ;

								if (v101 == 0) iCubeIndex |= 32;
								if (v111 == 0) iCubeIndex |= 128;
							}
							else //previous X not available
							{
								const uint8_t v001 = volIter.peekVoxel0px0py1pz();
								const uint8_t v101 = volIter.peekVoxel1px0py1pz();
								const uint8_t v011 = volIter.peekVoxel0px1py1pz();
								const uint8_t v111 = volIter.peekVoxel1px1py1pz();	

								//z
								uint8_t iPreviousCubeIndexZ = previousBitmask[getIndex(uXRegSpace,uYRegSpace, regSlice.width()+1)];
								iCubeIndex = iPreviousCubeIndexZ >> 4;

								if (v001 == 0) iCubeIndex |= 16;
								if (v101 == 0) iCubeIndex |= 32;
								if (v011 == 0) iCubeIndex |= 64;
								if (v111 == 0) iCubeIndex |= 128;
							}
						}
					}
					else //previous Z not available
					{
						if(isPrevYAvail)
						{
							if(isPrevXAvail)
							{
								const uint8_t v110 = volIter.peekVoxel1px1py0pz();
								const uint8_t v111 = volIter.peekVoxel1px1py1pz();

								//y
								uint8_t iPreviousCubeIndexY = bitmask[getIndex(uXRegSpace,uYRegSpace-1, regSlice.width()+1)];
								iPreviousCubeIndexY &= 204; //204 = 128+64+8+4
								iPreviousCubeIndexY >>= 2;

								//x
								uint8_t iPreviousCubeIndexX = bitmask[getIndex(uXRegSpace-1,uYRegSpace, regSlice.width()+1)];
								iPreviousCubeIndexX &= 170; //170 = 128+32+8+2
								iPreviousCubeIndexX >>= 1;

								iCubeIndex = iPreviousCubeIndexX | iPreviousCubeIndexY;

								if (v110 == 0) iCubeIndex |= 8;
								if (v111 == 0) iCubeIndex |= 128;
							}
							else //previous X not available
							{
								const uint8_t v010 = volIter.peekVoxel0px1py0pz();
								const uint8_t v110 = volIter.peekVoxel1px1py0pz();

								const uint8_t v011 = volIter.peekVoxel0px1py1pz();
								const uint8_t v111 = volIter.peekVoxel1px1py1pz();

								//y
								uint8_t iPreviousCubeIndexY = bitmask[getIndex(uXRegSpace,uYRegSpace-1, regSlice.width()+1)];
								iPreviousCubeIndexY &= 204; //204 = 128+64+8+4
								iPreviousCubeIndexY >>= 2;

								iCubeIndex = iPreviousCubeIndexY;

								if (v010 == 0) iCubeIndex |= 4;
								if (v110 == 0) iCubeIndex |= 8;
								if (v011 == 0) iCubeIndex |= 64;
								if (v111 == 0) iCubeIndex |= 128;
							}
						}
						else //previous Y not available
						{
							if(isPrevXAvail)
							{
								const uint8_t v100 = volIter.peekVoxel1px0py0pz();
								const uint8_t v110 = volIter.peekVoxel1px1py0pz();

								const uint8_t v101 = volIter.peekVoxel1px0py1pz();
								const uint8_t v111 = volIter.peekVoxel1px1py1pz();			

								//x
								uint8_t iPreviousCubeIndexX = bitmask[getIndex(uXRegSpace-1,uYRegSpace, regSlice.width()+1)];
								iPreviousCubeIndexX &= 170; //170 = 128+32+8+2
								iPreviousCubeIndexX >>= 1;

								iCubeIndex = iPreviousCubeIndexX;

								if (v100 == 0) iCubeIndex |= 2;	
								if (v110 == 0) iCubeIndex |= 8;
								if (v101 == 0) iCubeIndex |= 32;
								if (v111 == 0) iCubeIndex |= 128;
							}
							else //previous X not available
							{
								const uint8_t v000 = volIter.getVoxel();
								const uint8_t v100 = volIter.peekVoxel1px0py0pz();
								const uint8_t v010 = volIter.peekVoxel0px1py0pz();
								const uint8_t v110 = volIter.peekVoxel1px1py0pz();

								const uint8_t v001 = volIter.peekVoxel0px0py1pz();
								const uint8_t v101 = volIter.peekVoxel1px0py1pz();
								const uint8_t v011 = volIter.peekVoxel0px1py1pz();
								const uint8_t v111 = volIter.peekVoxel1px1py1pz();					

								if (v000 == 0) iCubeIndex |= 1;
								if (v100 == 0) iCubeIndex |= 2;
								if (v010 == 0) iCubeIndex |= 4;
								if (v110 == 0) iCubeIndex |= 8;
								if (v001 == 0) iCubeIndex |= 16;
								if (v101 == 0) iCubeIndex |= 32;
								if (v011 == 0) iCubeIndex |= 64;
								if (v111 == 0) iCubeIndex |= 128;
							}
						}
					}
				}
				else //We're at the edge of the volume - use bounds checking.
				{	
					const uint8_t v000 = volIter.getVoxel();
					const uint8_t v100 = volIter.getVolume().getVoxelAtWithBoundCheck(uXVolSpace+1, uYVolSpace  , uZVolSpace  );
					const uint8_t v010 = volIter.getVolume().getVoxelAtWithBoundCheck(uXVolSpace  , uYVolSpace+1, uZVolSpace  );
					const uint8_t v110 = volIter.getVolume().getVoxelAtWithBoundCheck(uXVolSpace+1, uYVolSpace+1, uZVolSpace  );

					const uint8_t v001 = volIter.getVolume().getVoxelAtWithBoundCheck(uXVolSpace  , uYVolSpace  , uZVolSpace+1);
					const uint8_t v101 = volIter.getVolume().getVoxelAtWithBoundCheck(uXVolSpace+1, uYVolSpace  , uZVolSpace+1);
					const uint8_t v011 = volIter.getVolume().getVoxelAtWithBoundCheck(uXVolSpace  , uYVolSpace+1, uZVolSpace+1);
					const uint8_t v111 = volIter.getVolume().getVoxelAtWithBoundCheck(uXVolSpace+1, uYVolSpace+1, uZVolSpace+1);				

					if (v000 == 0) iCubeIndex |= 1;
					if (v100 == 0) iCubeIndex |= 2;
					if (v010 == 0) iCubeIndex |= 4;
					if (v110 == 0) iCubeIndex |= 8;
					if (v001 == 0) iCubeIndex |= 16;
					if (v101 == 0) iCubeIndex |= 32;
					if (v011 == 0) iCubeIndex |= 64;
					if (v111 == 0) iCubeIndex |= 128;
				}

				//Save the bitmask
				bitmask[getIndex(uXRegSpace,uYRegSpace, regSlice.width()+1)] = iCubeIndex;

				if(edgeTable[iCubeIndex] != 0)
				{
					++uNoOfNonEmptyCells;
				}

			}
		}

		return uNoOfNonEmptyCells;
	}*/

	////////////////////////////////////////////////////////////////////////////////
	// Level 1
	////////////////////////////////////////////////////////////////////////////////

	void SurfaceExtractor::extractDecimatedSurfaceImpl(Volume<uint8_t>* volumeData, uint8_t uLevel, Region region, IndexedSurfacePatch* singleMaterialPatch)
	{	
		singleMaterialPatch->clear();

		//For edge indices
		//FIXME - do the slices need to be this big? Surely for a decimated mesh they can be smaller?
		//FIXME - Instead of region.width()+2 we used to use POLYVOX_REGION_SIDE_LENGTH+1
		//Normally POLYVOX_REGION_SIDE_LENGTH is the same as region.width() (often 32) but at the
		//edges of the volume it is 1 smaller. Need to think what values really belong here.
		int32_t* vertexIndicesX0 = new int32_t[(region.width()+2) * (region.height()+2)];
		int32_t* vertexIndicesY0 = new int32_t[(region.width()+2) * (region.height()+2)];
		int32_t* vertexIndicesZ0 = new int32_t[(region.width()+2) * (region.height()+2)];
		int32_t* vertexIndicesX1 = new int32_t[(region.width()+2) * (region.height()+2)];
		int32_t* vertexIndicesY1 = new int32_t[(region.width()+2) * (region.height()+2)];
		int32_t* vertexIndicesZ1 = new int32_t[(region.width()+2) * (region.height()+2)];

		//Cell bitmasks
		uint8_t* bitmask0 = new uint8_t[(region.width()+2) * (region.height()+2)];
		uint8_t* bitmask1 = new uint8_t[(region.width()+2) * (region.height()+2)];

		const uint8_t uStepSize = uLevel == 0 ? 1 : 1 << uLevel;

		//When generating the mesh for a region we actually look outside it in the
		// back, bottom, right direction. Protect against access violations by cropping region here
		Region regVolume = volumeData->getEnclosingRegion();
		regVolume.setUpperCorner(regVolume.getUpperCorner() - Vector3DInt32(2*uStepSize-1,2*uStepSize-1,2*uStepSize-1));
		region.cropTo(regVolume);

		//Offset from volume corner
		const Vector3DFloat offset = static_cast<Vector3DFloat>(region.getLowerCorner());

		//Create a region corresponding to the first slice
		Region regSlice0(region);
		Vector3DInt32 v3dUpperCorner = regSlice0.getUpperCorner();
		v3dUpperCorner.setZ(regSlice0.getLowerCorner().getZ()); //Set the upper z to the lower z to make it one slice thick.
		regSlice0.setUpperCorner(v3dUpperCorner);
		
		//Iterator to access the volume data
		VolumeSampler<uint8_t> volIter(*volumeData);		

		//Compute bitmask for initial slice
		uint32_t uNoOfNonEmptyCellsForSlice0 = computeBitmaskForSlice(volIter, uLevel, regSlice0, offset, bitmask0, 0);		
		if(uNoOfNonEmptyCellsForSlice0 != 0)
		{
			//If there were some non-empty cells then generate initial slice vertices for them
			generateVerticesForSlice(volIter, uLevel, regSlice0, offset, bitmask0, singleMaterialPatch, vertexIndicesX0, vertexIndicesY0, vertexIndicesZ0);
		}

		for(uint32_t uSlice = 1; ((uSlice <= region.depth()) && (uSlice + offset.getZ() <= regVolume.getUpperCorner().getZ())); uSlice += uStepSize)
		{
			Region regSlice1(regSlice0);
			regSlice1.shift(Vector3DInt32(0,0,uStepSize));

			uint32_t uNoOfNonEmptyCellsForSlice1 = computeBitmaskForSlice(volIter, uLevel, regSlice1, offset, bitmask1, bitmask0);

			if(uNoOfNonEmptyCellsForSlice1 != 0)
			{
				generateVerticesForSlice(volIter, uLevel, regSlice1, offset, bitmask1, singleMaterialPatch, vertexIndicesX1, vertexIndicesY1, vertexIndicesZ1);				
			}

			if((uNoOfNonEmptyCellsForSlice0 != 0) || (uNoOfNonEmptyCellsForSlice1 != 0))
			{
				generateIndicesForSlice(volIter, uLevel, regSlice0, singleMaterialPatch, offset, bitmask0, bitmask1, vertexIndicesX0, vertexIndicesY0, vertexIndicesZ0, vertexIndicesX1, vertexIndicesY1, vertexIndicesZ1);
			}

			std::swap(uNoOfNonEmptyCellsForSlice0, uNoOfNonEmptyCellsForSlice1);
			std::swap(bitmask0, bitmask1);
			std::swap(vertexIndicesX0, vertexIndicesX1);
			std::swap(vertexIndicesY0, vertexIndicesY1);
			std::swap(vertexIndicesZ0, vertexIndicesZ1);

			regSlice0 = regSlice1;
		}

		delete[] bitmask0;
		delete[] bitmask1;
		delete[] vertexIndicesX0;
		delete[] vertexIndicesX1;
		delete[] vertexIndicesY0;
		delete[] vertexIndicesY1;
		delete[] vertexIndicesZ0;
		delete[] vertexIndicesZ1;


		/*std::vector<SurfaceVertex>::iterator iterSurfaceVertex = singleMaterialPatch->getVertices().begin();
		while(iterSurfaceVertex != singleMaterialPatch->getVertices().end())
		{
			Vector3DFloat tempNormal = computeDecimatedNormal(volumeData, static_cast<Vector3DFloat>(iterSurfaceVertex->getPosition() + offset), CENTRAL_DIFFERENCE);
			const_cast<SurfaceVertex&>(*iterSurfaceVertex).setNormal(tempNormal);
			++iterSurfaceVertex;
		}*/
	}

	uint32_t SurfaceExtractor::computeBitmaskForSlice(VolumeSampler<uint8_t>& volIter, uint8_t uLevel, const Region& regSlice, const Vector3DFloat& offset, uint8_t* bitmask, uint8_t* previousBitmask)
	{
		const uint8_t uStepSize = uLevel == 0 ? 1 : 1 << uLevel;
		uint32_t uNoOfNonEmptyCells = 0;

		//Iterate over each cell in the region
		for(uint16_t uYVolSpace = regSlice.getLowerCorner().getY(); uYVolSpace <= regSlice.getUpperCorner().getY(); uYVolSpace += uStepSize)
		{
			for(uint16_t uXVolSpace = regSlice.getLowerCorner().getX(); uXVolSpace <= regSlice.getUpperCorner().getX(); uXVolSpace += uStepSize)
			{	
				uint16_t uZVolSpace = regSlice.getLowerCorner().getZ();
				//Current position
				volIter.setPosition(uXVolSpace,uYVolSpace,uZVolSpace);

				const uint16_t uXRegSpace = uXVolSpace - offset.getX();
				const uint16_t uYRegSpace = uYVolSpace - offset.getY();

				//Determine the index into the edge table which tells us which vertices are inside of the surface
				uint8_t iCubeIndex = 0;

				if((uXVolSpace < volIter.getVolume().getWidth()-uStepSize) &&
					(uYVolSpace < volIter.getVolume().getHeight()-uStepSize) &&
					(uZVolSpace < volIter.getVolume().getDepth()-uStepSize))
				{
				bool isPrevXAvail = uXRegSpace > 0;
				bool isPrevYAvail = uYRegSpace > 0;
				bool isPrevZAvail = previousBitmask != 0;

				if(isPrevZAvail)
				{
					if(isPrevYAvail)
					{
						if(isPrevXAvail)
						{
							volIter.setPosition(uXVolSpace+uStepSize,uYVolSpace+uStepSize,uZVolSpace+uStepSize);
							const uint8_t v111 = volIter.getSubSampledVoxel(uLevel);	

							//z
							uint8_t iPreviousCubeIndexZ = previousBitmask[getIndex(uXRegSpace,uYRegSpace, regSlice.width()+1)];
							iPreviousCubeIndexZ >>= 4;

							//y
							uint8_t iPreviousCubeIndexY = bitmask[getIndex(uXRegSpace,uYRegSpace-uStepSize, regSlice.width()+1)];
							iPreviousCubeIndexY &= 192; //192 = 128 + 64
							iPreviousCubeIndexY >>= 2;

							//x
							uint8_t iPreviousCubeIndexX = bitmask[getIndex(uXRegSpace-uStepSize,uYRegSpace, regSlice.width()+1)];
							iPreviousCubeIndexX &= 128;
							iPreviousCubeIndexX >>= 1;

							iCubeIndex = iPreviousCubeIndexX | iPreviousCubeIndexY | iPreviousCubeIndexZ;

							if (v111 == 0) iCubeIndex |= 128;
						}
						else //previous X not available
						{
							volIter.setPosition(uXVolSpace,uYVolSpace+uStepSize,uZVolSpace+uStepSize);
							const uint8_t v011 = volIter.getSubSampledVoxel(uLevel);
							volIter.setPosition(uXVolSpace+uStepSize,uYVolSpace+uStepSize,uZVolSpace+uStepSize);
							const uint8_t v111 = volIter.getSubSampledVoxel(uLevel);	

							//z
							uint8_t iPreviousCubeIndexZ = previousBitmask[getIndex(uXRegSpace,uYRegSpace, regSlice.width()+1)];
							iPreviousCubeIndexZ >>= 4;

							//y
							uint8_t iPreviousCubeIndexY = bitmask[getIndex(uXRegSpace,uYRegSpace-uStepSize, regSlice.width()+1)];
							iPreviousCubeIndexY &= 192; //192 = 128 + 64
							iPreviousCubeIndexY >>= 2;

							iCubeIndex = iPreviousCubeIndexY | iPreviousCubeIndexZ;

							if (v011 == 0) iCubeIndex |= 64;
							if (v111 == 0) iCubeIndex |= 128;
						}
					}
					else //previous Y not available
					{
						if(isPrevXAvail)
						{
							volIter.setPosition(uXVolSpace+uStepSize,uYVolSpace,uZVolSpace+uStepSize);
							const uint8_t v101 = volIter.getSubSampledVoxel(uLevel);
							volIter.setPosition(uXVolSpace+uStepSize,uYVolSpace+uStepSize,uZVolSpace+uStepSize);
							const uint8_t v111 = volIter.getSubSampledVoxel(uLevel);	

							//z
							uint8_t iPreviousCubeIndexZ = previousBitmask[getIndex(uXRegSpace,uYRegSpace, regSlice.width()+1)];
							iPreviousCubeIndexZ >>= 4;

							//x
							uint8_t iPreviousCubeIndexX = bitmask[getIndex(uXRegSpace-uStepSize,uYRegSpace, regSlice.width()+1)];
							iPreviousCubeIndexX &= 160; //160 = 128+32
							iPreviousCubeIndexX >>= 1;

							iCubeIndex = iPreviousCubeIndexX | iPreviousCubeIndexZ;

							if (v101 == 0) iCubeIndex |= 32;
							if (v111 == 0) iCubeIndex |= 128;
						}
						else //previous X not available
						{
							volIter.setPosition(uXVolSpace,uYVolSpace,uZVolSpace+uStepSize);
							const uint8_t v001 = volIter.getSubSampledVoxel(uLevel);
							volIter.setPosition(uXVolSpace+uStepSize,uYVolSpace,uZVolSpace+uStepSize);
							const uint8_t v101 = volIter.getSubSampledVoxel(uLevel);
							volIter.setPosition(uXVolSpace,uYVolSpace+uStepSize,uZVolSpace+uStepSize);
							const uint8_t v011 = volIter.getSubSampledVoxel(uLevel);
							volIter.setPosition(uXVolSpace+uStepSize,uYVolSpace+uStepSize,uZVolSpace+uStepSize);
							const uint8_t v111 = volIter.getSubSampledVoxel(uLevel);	

							//z
							uint8_t iPreviousCubeIndexZ = previousBitmask[getIndex(uXRegSpace,uYRegSpace, regSlice.width()+1)];
							iCubeIndex = iPreviousCubeIndexZ >> 4;

							if (v001 == 0) iCubeIndex |= 16;
							if (v101 == 0) iCubeIndex |= 32;
							if (v011 == 0) iCubeIndex |= 64;
							if (v111 == 0) iCubeIndex |= 128;
						}
					}
				}
				else //previous Z not available
				{
					if(isPrevYAvail)
					{
						if(isPrevXAvail)
						{
							volIter.setPosition(uXVolSpace+uStepSize,uYVolSpace+uStepSize,uZVolSpace);
							const uint8_t v110 = volIter.getSubSampledVoxel(uLevel);

							volIter.setPosition(uXVolSpace+uStepSize,uYVolSpace+uStepSize,uZVolSpace+uStepSize);
							const uint8_t v111 = volIter.getSubSampledVoxel(uLevel);

							//y
							uint8_t iPreviousCubeIndexY = bitmask[getIndex(uXRegSpace,uYRegSpace-uStepSize, regSlice.width()+1)];
							iPreviousCubeIndexY &= 204; //204 = 128+64+8+4
							iPreviousCubeIndexY >>= 2;

							//x
							uint8_t iPreviousCubeIndexX = bitmask[getIndex(uXRegSpace-uStepSize,uYRegSpace, regSlice.width()+1)];
							iPreviousCubeIndexX &= 170; //170 = 128+32+8+2
							iPreviousCubeIndexX >>= 1;

							iCubeIndex = iPreviousCubeIndexX | iPreviousCubeIndexY;

							if (v110 == 0) iCubeIndex |= 8;
							if (v111 == 0) iCubeIndex |= 128;
						}
						else //previous X not available
						{
							volIter.setPosition(uXVolSpace,uYVolSpace+uStepSize,uZVolSpace);
							const uint8_t v010 = volIter.getSubSampledVoxel(uLevel);
							volIter.setPosition(uXVolSpace+uStepSize,uYVolSpace+uStepSize,uZVolSpace);
							const uint8_t v110 = volIter.getSubSampledVoxel(uLevel);

							volIter.setPosition(uXVolSpace,uYVolSpace+uStepSize,uZVolSpace+uStepSize);
							const uint8_t v011 = volIter.getSubSampledVoxel(uLevel);
							volIter.setPosition(uXVolSpace+uStepSize,uYVolSpace+uStepSize,uZVolSpace+uStepSize);
							const uint8_t v111 = volIter.getSubSampledVoxel(uLevel);

							//y
							uint8_t iPreviousCubeIndexY = bitmask[getIndex(uXRegSpace,uYRegSpace-uStepSize, regSlice.width()+1)];
							iPreviousCubeIndexY &= 204; //204 = 128+64+8+4
							iPreviousCubeIndexY >>= 2;

							iCubeIndex = iPreviousCubeIndexY;

							if (v010 == 0) iCubeIndex |= 4;
							if (v110 == 0) iCubeIndex |= 8;
							if (v011 == 0) iCubeIndex |= 64;
							if (v111 == 0) iCubeIndex |= 128;
						}
					}
					else //previous Y not available
					{
						if(isPrevXAvail)
						{
							volIter.setPosition(uXVolSpace+uStepSize,uYVolSpace,uZVolSpace);
							const uint8_t v100 = volIter.getSubSampledVoxel(uLevel);
							volIter.setPosition(uXVolSpace+uStepSize,uYVolSpace+uStepSize,uZVolSpace);
							const uint8_t v110 = volIter.getSubSampledVoxel(uLevel);

							volIter.setPosition(uXVolSpace+uStepSize,uYVolSpace,uZVolSpace+uStepSize);
							const uint8_t v101 = volIter.getSubSampledVoxel(uLevel);
							volIter.setPosition(uXVolSpace+uStepSize,uYVolSpace+uStepSize,uZVolSpace+uStepSize);
							const uint8_t v111 = volIter.getSubSampledVoxel(uLevel);

							//x
							uint8_t iPreviousCubeIndexX = bitmask[getIndex(uXRegSpace-uStepSize,uYRegSpace, regSlice.width()+1)];
							iPreviousCubeIndexX &= 170; //170 = 128+32+8+2
							iPreviousCubeIndexX >>= 1;

							iCubeIndex = iPreviousCubeIndexX;

							if (v100 == 0) iCubeIndex |= 2;	
							if (v110 == 0) iCubeIndex |= 8;
							if (v101 == 0) iCubeIndex |= 32;
							if (v111 == 0) iCubeIndex |= 128;
						}
						else //previous X not available
						{
							volIter.setPosition(uXVolSpace,uYVolSpace,uZVolSpace);
							const uint8_t v000 = volIter.getSubSampledVoxel(uLevel);
							volIter.setPosition(uXVolSpace+uStepSize,uYVolSpace,uZVolSpace);
							const uint8_t v100 = volIter.getSubSampledVoxel(uLevel);
							volIter.setPosition(uXVolSpace,uYVolSpace+uStepSize,uZVolSpace);
							const uint8_t v010 = volIter.getSubSampledVoxel(uLevel);
							volIter.setPosition(uXVolSpace+uStepSize,uYVolSpace+uStepSize,uZVolSpace);
							const uint8_t v110 = volIter.getSubSampledVoxel(uLevel);

							volIter.setPosition(uXVolSpace,uYVolSpace,uZVolSpace+uStepSize);
							const uint8_t v001 = volIter.getSubSampledVoxel(uLevel);
							volIter.setPosition(uXVolSpace+uStepSize,uYVolSpace,uZVolSpace+uStepSize);
							const uint8_t v101 = volIter.getSubSampledVoxel(uLevel);
							volIter.setPosition(uXVolSpace,uYVolSpace+uStepSize,uZVolSpace+uStepSize);
							const uint8_t v011 = volIter.getSubSampledVoxel(uLevel);
							volIter.setPosition(uXVolSpace+uStepSize,uYVolSpace+uStepSize,uZVolSpace+uStepSize);
							const uint8_t v111 = volIter.getSubSampledVoxel(uLevel);		

							if (v000 == 0) iCubeIndex |= 1;
							if (v100 == 0) iCubeIndex |= 2;
							if (v010 == 0) iCubeIndex |= 4;
							if (v110 == 0) iCubeIndex |= 8;
							if (v001 == 0) iCubeIndex |= 16;
							if (v101 == 0) iCubeIndex |= 32;
							if (v011 == 0) iCubeIndex |= 64;
							if (v111 == 0) iCubeIndex |= 128;
						}
					}
				}
				}
				else
				{
					if(uLevel == 0)
					{
						const uint8_t v000 = volIter.getVoxel();
						const uint8_t v100 = volIter.getVolume().getVoxelAtWithBoundCheck(uXVolSpace+1, uYVolSpace  , uZVolSpace  );
						const uint8_t v010 = volIter.getVolume().getVoxelAtWithBoundCheck(uXVolSpace  , uYVolSpace+1, uZVolSpace  );
						const uint8_t v110 = volIter.getVolume().getVoxelAtWithBoundCheck(uXVolSpace+1, uYVolSpace+1, uZVolSpace  );

						const uint8_t v001 = volIter.getVolume().getVoxelAtWithBoundCheck(uXVolSpace  , uYVolSpace  , uZVolSpace+1);
						const uint8_t v101 = volIter.getVolume().getVoxelAtWithBoundCheck(uXVolSpace+1, uYVolSpace  , uZVolSpace+1);
						const uint8_t v011 = volIter.getVolume().getVoxelAtWithBoundCheck(uXVolSpace  , uYVolSpace+1, uZVolSpace+1);
						const uint8_t v111 = volIter.getVolume().getVoxelAtWithBoundCheck(uXVolSpace+1, uYVolSpace+1, uZVolSpace+1);

						if (v000 == 0) iCubeIndex |= 1;
						if (v100 == 0) iCubeIndex |= 2;
						if (v010 == 0) iCubeIndex |= 4;
						if (v110 == 0) iCubeIndex |= 8;
						if (v001 == 0) iCubeIndex |= 16;
						if (v101 == 0) iCubeIndex |= 32;
						if (v011 == 0) iCubeIndex |= 64;
						if (v111 == 0) iCubeIndex |= 128;
					}
					else
					{
						const uint8_t v000 = volIter.getSubSampledVoxelWithBoundsCheck(uLevel);

						volIter.setPosition(uXVolSpace+1, uYVolSpace  , uZVolSpace  );
						const uint8_t v100 = volIter.getSubSampledVoxelWithBoundsCheck(uLevel);

						volIter.setPosition(uXVolSpace  , uYVolSpace+1, uZVolSpace  );
						const uint8_t v010 = volIter.getSubSampledVoxelWithBoundsCheck(uLevel);

						volIter.setPosition(uXVolSpace+1, uYVolSpace+1, uZVolSpace  );
						const uint8_t v110 = volIter.getSubSampledVoxelWithBoundsCheck(uLevel);


						volIter.setPosition(uXVolSpace  , uYVolSpace  , uZVolSpace+1);
						const uint8_t v001 = volIter.getSubSampledVoxelWithBoundsCheck(uLevel);

						volIter.setPosition(uXVolSpace+1, uYVolSpace  , uZVolSpace+1);
						const uint8_t v101 = volIter.getSubSampledVoxelWithBoundsCheck(uLevel);

						volIter.setPosition(uXVolSpace  , uYVolSpace+1, uZVolSpace+1);
						const uint8_t v011 = volIter.getSubSampledVoxelWithBoundsCheck(uLevel);

						volIter.setPosition(uXVolSpace+1, uYVolSpace+1, uZVolSpace+1);				
						const uint8_t v111 = volIter.getSubSampledVoxelWithBoundsCheck(uLevel);				

						if (v000 == 0) iCubeIndex |= 1;
						if (v100 == 0) iCubeIndex |= 2;
						if (v010 == 0) iCubeIndex |= 4;
						if (v110 == 0) iCubeIndex |= 8;
						if (v001 == 0) iCubeIndex |= 16;
						if (v101 == 0) iCubeIndex |= 32;
						if (v011 == 0) iCubeIndex |= 64;
						if (v111 == 0) iCubeIndex |= 128;
					}
				}

				//Save the bitmask
				bitmask[getIndex(uXRegSpace,uYVolSpace- offset.getY(), regSlice.width()+1)] = iCubeIndex;

				if(edgeTable[iCubeIndex] != 0)
				{
					++uNoOfNonEmptyCells;
				}
				
			}//For each cell
		}

		return uNoOfNonEmptyCells;
	}

	void SurfaceExtractor::generateVerticesForSlice(VolumeSampler<uint8_t>& volIter, uint8_t uLevel, Region& regSlice, const Vector3DFloat& offset, uint8_t* bitmask, IndexedSurfacePatch* singleMaterialPatch,int32_t vertexIndicesX[],int32_t vertexIndicesY[],int32_t vertexIndicesZ[])
	{
		const uint8_t uStepSize = uLevel == 0 ? 1 : 1 << uLevel;

		//Iterate over each cell in the region
		for(uint16_t uYVolSpace = regSlice.getLowerCorner().getY(); uYVolSpace <= regSlice.getUpperCorner().getY(); uYVolSpace += uStepSize)
		{
			for(uint16_t uXVolSpace = regSlice.getLowerCorner().getX(); uXVolSpace <= regSlice.getUpperCorner().getX(); uXVolSpace += uStepSize)
			{		
				uint16_t uZVolSpace = regSlice.getLowerCorner().getZ();

				//Current position
				const uint16_t uXRegSpace = uXVolSpace - offset.getX();
				const uint16_t uYRegSpace = uYVolSpace - offset.getY();
				const uint16_t uZRegSpace = uZVolSpace - offset.getZ();

				//Current position
				//const uint16_t z = regSlice.getLowerCorner().getZ();

				volIter.setPosition(uXVolSpace,uYVolSpace,uZVolSpace);
				const uint8_t v000 = volIter.getSubSampledVoxel(uLevel);

				//Determine the index into the edge table which tells us which vertices are inside of the surface
				uint8_t iCubeIndex = bitmask[getIndex(uXVolSpace - offset.getX(),uYVolSpace - offset.getY(), regSlice.width()+1)];

				/* Cube is entirely in/out of the surface */
				if (edgeTable[iCubeIndex] == 0)
				{
					continue;
				}

				/* Find the vertices where the surface intersects the cube */
				if (edgeTable[iCubeIndex] & 1)
				{
					if(uXVolSpace != regSlice.getUpperCorner().getX())
					{
						volIter.setPosition(uXVolSpace + uStepSize,uYVolSpace,uZVolSpace);
						const uint8_t v100 = volIter.getSubSampledVoxel(uLevel);
						const Vector3DFloat v3dPosition(uXVolSpace - offset.getX() + 0.5f * uStepSize, uYVolSpace - offset.getY(), uZVolSpace - offset.getZ());
						const Vector3DFloat v3dNormal(v000 > v100 ? 1.0f : -1.0f,0.0,0.0);
						const uint8_t uMaterial = v000 | v100; //Because one of these is 0, the or operation takes the max.
						SurfaceVertex surfaceVertex(v3dPosition, v3dNormal, uMaterial);
						uint32_t uLastVertexIndex = singleMaterialPatch->addVertex(surfaceVertex);
						vertexIndicesX[getIndex(uXVolSpace - offset.getX(),uYVolSpace - offset.getY(), regSlice.width()+1)] = uLastVertexIndex;
					}
				}
				if (edgeTable[iCubeIndex] & 8)
				{
					if(uYVolSpace != regSlice.getUpperCorner().getY())
					{
						volIter.setPosition(uXVolSpace,uYVolSpace + uStepSize,uZVolSpace);
						const uint8_t v010 = volIter.getSubSampledVoxel(uLevel);
						const Vector3DFloat v3dPosition(uXVolSpace - offset.getX(), uYVolSpace - offset.getY() + 0.5f * uStepSize, uZVolSpace - offset.getZ());
						const Vector3DFloat v3dNormal(0.0,v000 > v010 ? 1.0f : -1.0f,0.0);
						const uint8_t uMaterial = v000 | v010; //Because one of these is 0, the or operation takes the max.
						SurfaceVertex surfaceVertex(v3dPosition, v3dNormal, uMaterial);
						uint32_t uLastVertexIndex = singleMaterialPatch->addVertex(surfaceVertex);
						vertexIndicesY[getIndex(uXVolSpace - offset.getX(),uYVolSpace - offset.getY(), regSlice.width()+1)] = uLastVertexIndex;
					}
				}
				if (edgeTable[iCubeIndex] & 256)
				{
					//if(z != regSlice.getUpperCorner.getZ())
					{
						volIter.setPosition(uXVolSpace,uYVolSpace,uZVolSpace + uStepSize);
						const uint8_t v001 = volIter.getSubSampledVoxel(uLevel);
						const Vector3DFloat v3dPosition(uXVolSpace - offset.getX(), uYVolSpace - offset.getY(), uZVolSpace - offset.getZ() + 0.5f * uStepSize);
						const Vector3DFloat v3dNormal(0.0,0.0,v000 > v001 ? 1.0f : -1.0f);
						const uint8_t uMaterial = v000 | v001; //Because one of these is 0, the or operation takes the max.
						const SurfaceVertex surfaceVertex(v3dPosition, v3dNormal, uMaterial);
						uint32_t uLastVertexIndex = singleMaterialPatch->addVertex(surfaceVertex);
						vertexIndicesZ[getIndex(uXVolSpace - offset.getX(),uYVolSpace - offset.getY(), regSlice.width()+1)] = uLastVertexIndex;
					}
				}
			}//For each cell
		}
	}

	void SurfaceExtractor::generateIndicesForSlice(VolumeSampler<uint8_t>& volIter, uint8_t uLevel, const Region& regSlice, IndexedSurfacePatch* singleMaterialPatch, const Vector3DFloat& offset, uint8_t* bitmask0, uint8_t* bitmask1, int32_t vertexIndicesX0[],int32_t vertexIndicesY0[],int32_t vertexIndicesZ0[], int32_t vertexIndicesX1[],int32_t vertexIndicesY1[],int32_t vertexIndicesZ1[])
	{
		const uint8_t uStepSize = uLevel == 0 ? 1 : 1 << uLevel;
		uint32_t indlist[12];

		for(uint16_t uYVolSpace = regSlice.getLowerCorner().getY(); uYVolSpace < regSlice.getUpperCorner().getY(); uYVolSpace += uStepSize)
		{
			for(uint16_t uXVolSpace = regSlice.getLowerCorner().getX(); uXVolSpace < regSlice.getUpperCorner().getX(); uXVolSpace += uStepSize)
			{		
				uint16_t uZVolSpace = regSlice.getLowerCorner().getZ();
				volIter.setPosition(uXVolSpace,uYVolSpace,uZVolSpace);	

				//Current position
				const uint16_t uXRegSpace = volIter.getPosX() - offset.getX();
				const uint16_t uYRegSpace = volIter.getPosY() - offset.getY();
				const uint16_t uZRegSpace = volIter.getPosZ() - offset.getZ();

				//Determine the index into the edge table which tells us which vertices are inside of the surface
				uint8_t iCubeIndex = bitmask0[getIndex(uXRegSpace,uYRegSpace, regSlice.width()+1)];

				/* Cube is entirely in/out of the surface */
				if (edgeTable[iCubeIndex] == 0)
				{
					continue;
				}

				/* Find the vertices where the surface intersects the cube */
				if (edgeTable[iCubeIndex] & 1)
				{
					indlist[0] = vertexIndicesX0[getIndex(uXRegSpace,uYRegSpace, regSlice.width()+1)];
					assert(indlist[0] != -1);
				}
				if (edgeTable[iCubeIndex] & 2)
				{
					indlist[1] = vertexIndicesY0[getIndex(uXRegSpace+uStepSize,uYRegSpace, regSlice.width()+1)];
					assert(indlist[1] != -1);
				}
				if (edgeTable[iCubeIndex] & 4)
				{
					indlist[2] = vertexIndicesX0[getIndex(uXRegSpace,uYRegSpace+uStepSize, regSlice.width()+1)];
					assert(indlist[2] != -1);
				}
				if (edgeTable[iCubeIndex] & 8)
				{
					indlist[3] = vertexIndicesY0[getIndex(uXRegSpace,uYRegSpace, regSlice.width()+1)];
					assert(indlist[3] != -1);
				}
				if (edgeTable[iCubeIndex] & 16)
				{
					indlist[4] = vertexIndicesX1[getIndex(uXRegSpace,uYRegSpace, regSlice.width()+1)];
					assert(indlist[4] != -1);
				}
				if (edgeTable[iCubeIndex] & 32)
				{
					indlist[5] = vertexIndicesY1[getIndex(uXRegSpace+uStepSize,uYRegSpace, regSlice.width()+1)];
					assert(indlist[5] != -1);
				}
				if (edgeTable[iCubeIndex] & 64)
				{
					indlist[6] = vertexIndicesX1[getIndex(uXRegSpace,uYRegSpace+uStepSize, regSlice.width()+1)];
					assert(indlist[6] != -1);
				}
				if (edgeTable[iCubeIndex] & 128)
				{
					indlist[7] = vertexIndicesY1[getIndex(uXRegSpace,uYRegSpace, regSlice.width()+1)];
					assert(indlist[7] != -1);
				}
				if (edgeTable[iCubeIndex] & 256)
				{
					indlist[8] = vertexIndicesZ0[getIndex(uXRegSpace,uYRegSpace, regSlice.width()+1)];
					assert(indlist[8] != -1);
				}
				if (edgeTable[iCubeIndex] & 512)
				{
					indlist[9] = vertexIndicesZ0[getIndex(uXRegSpace+uStepSize,uYRegSpace, regSlice.width()+1)];
					assert(indlist[9] != -1);
				}
				if (edgeTable[iCubeIndex] & 1024)
				{
					indlist[10] = vertexIndicesZ0[getIndex(uXRegSpace+uStepSize,uYRegSpace+uStepSize, regSlice.width()+1)];
					assert(indlist[10] != -1);
				}
				if (edgeTable[iCubeIndex] & 2048)
				{
					indlist[11] = vertexIndicesZ0[getIndex(uXRegSpace,uYRegSpace+uStepSize, regSlice.width()+1)];
					assert(indlist[11] != -1);
				}

				for (int i=0;triTable[iCubeIndex][i]!=-1;i+=3)
				{
					uint32_t ind0 = indlist[triTable[iCubeIndex][i  ]];
					uint32_t ind1 = indlist[triTable[iCubeIndex][i+1]];
					uint32_t ind2 = indlist[triTable[iCubeIndex][i+2]];

					singleMaterialPatch->addTriangle(ind0, ind1, ind2);
				}//For each triangle
			}//For each cell
		}
	}
}