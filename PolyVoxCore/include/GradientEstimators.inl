#pragma region License
/******************************************************************************
This file is part of the PolyVox library
Copyright (C) 2006  David Williams

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
******************************************************************************/
#pragma endregion

#include "SurfaceAdjusters.h"

namespace PolyVox
{
	template <typename VoxelType>
	Vector3DFloat computeCentralDifferenceGradient(const BlockVolumeIterator<VoxelType>& volIter)
	{
		//FIXME - bitwise way of doing this?
		VoxelType voxel1nx = volIter.peekVoxel1nx0py0pz() > 0 ? 1: 0;
		VoxelType voxel1px = volIter.peekVoxel1px0py0pz() > 0 ? 1: 0;

		VoxelType voxel1ny = volIter.peekVoxel0px1ny0pz() > 0 ? 1: 0;
		VoxelType voxel1py = volIter.peekVoxel0px1py0pz() > 0 ? 1: 0;

		VoxelType voxel1nz = volIter.peekVoxel0px0py1nz() > 0 ? 1: 0;
		VoxelType voxel1pz = volIter.peekVoxel0px0py1pz() > 0 ? 1: 0;

		return Vector3DFloat
		(
			static_cast<float>(voxel1nx) - static_cast<float>(voxel1px),
			static_cast<float>(voxel1ny) - static_cast<float>(voxel1py),
			static_cast<float>(voxel1nz) - static_cast<float>(voxel1pz)
		);
	}

	template <typename VoxelType>
	Vector3DFloat computeDecimatedCentralDifferenceGradient(const BlockVolumeIterator<VoxelType>& volIter)
	{
		const uint16_t x = volIter.getPosX();
		const uint16_t y = volIter.getPosY();
		const uint16_t z = volIter.getPosZ();

		//FIXME - bitwise way of doing this?
		VoxelType voxel1nx = volIter.getVoxelAt(x-2, y  ,z  ) > 0 ? 1: 0;
		VoxelType voxel1px = volIter.getVoxelAt(x-2, y  ,z  ) > 0 ? 1: 0;

		VoxelType voxel1ny = volIter.getVoxelAt(x  , y-2,z  ) > 0 ? 1: 0;
		VoxelType voxel1py = volIter.getVoxelAt(x  , y-2,z  ) > 0 ? 1: 0;

		VoxelType voxel1nz = volIter.getVoxelAt(x  , y  ,z-2) > 0 ? 1: 0;
		VoxelType voxel1pz = volIter.getVoxelAt(x  , y  ,z-2) > 0 ? 1: 0;

		return Vector3DFloat
		(
			static_cast<float>(voxel1nx) - static_cast<float>(voxel1px),
			static_cast<float>(voxel1ny) - static_cast<float>(voxel1py),
			static_cast<float>(voxel1nz) - static_cast<float>(voxel1pz)
		);
	}

	template <typename VoxelType>
	Vector3DFloat computeSmoothCentralDifferenceGradient(BlockVolumeIterator<VoxelType>& volIter)
	{
		boost::uint16_t initialX = volIter.getPosX();
		boost::uint16_t initialY = volIter.getPosY();
		boost::uint16_t initialZ = volIter.getPosZ();

		//FIXME - bitwise way of doing this?
		volIter.setPosition(initialX-1, initialY, initialZ);
		float voxel1nx = computeSmoothedVoxel(volIter);
		volIter.setPosition(initialX+1, initialY, initialZ);
		float voxel1px = computeSmoothedVoxel(volIter);

		volIter.setPosition(initialX, initialY-1, initialZ);
		float voxel1ny = computeSmoothedVoxel(volIter);
		volIter.setPosition(initialX, initialY+1, initialZ);
		float voxel1py = computeSmoothedVoxel(volIter);

		volIter.setPosition(initialX, initialY, initialZ-1);
		float voxel1nz = computeSmoothedVoxel(volIter);
		volIter.setPosition(initialX, initialY, initialZ+1);
		float voxel1pz = computeSmoothedVoxel(volIter);

		return Vector3DFloat
		(
			voxel1nx - voxel1px,
			voxel1ny - voxel1py,
			voxel1nz - voxel1pz
		);
	}

	template <typename VoxelType>
	Vector3DFloat computeSobelGradient(const BlockVolumeIterator<VoxelType>& volIter)
	{
		static const int weights[3][3][3] = {  {  {2,3,2}, {3,6,3}, {2,3,2}  },  {
			{3,6,3},  {6,0,6},  {3,6,3} },  { {2,3,2},  {3,6,3},  {2,3,2} } };

			const VoxelType pVoxel1nx1ny1nz = volIter.peekVoxel1nx1ny1nz() > 0 ? 1: 0;
			const VoxelType pVoxel1nx1ny0pz = volIter.peekVoxel1nx1ny0pz() > 0 ? 1: 0;
			const VoxelType pVoxel1nx1ny1pz = volIter.peekVoxel1nx1ny1pz() > 0 ? 1: 0;
			const VoxelType pVoxel1nx0py1nz = volIter.peekVoxel1nx0py1nz() > 0 ? 1: 0;
			const VoxelType pVoxel1nx0py0pz = volIter.peekVoxel1nx0py0pz() > 0 ? 1: 0;
			const VoxelType pVoxel1nx0py1pz = volIter.peekVoxel1nx0py1pz() > 0 ? 1: 0;
			const VoxelType pVoxel1nx1py1nz = volIter.peekVoxel1nx1py1nz() > 0 ? 1: 0;
			const VoxelType pVoxel1nx1py0pz = volIter.peekVoxel1nx1py0pz() > 0 ? 1: 0;
			const VoxelType pVoxel1nx1py1pz = volIter.peekVoxel1nx1py1pz() > 0 ? 1: 0;

			const VoxelType pVoxel0px1ny1nz = volIter.peekVoxel0px1ny1nz() > 0 ? 1: 0;
			const VoxelType pVoxel0px1ny0pz = volIter.peekVoxel0px1ny0pz() > 0 ? 1: 0;
			const VoxelType pVoxel0px1ny1pz = volIter.peekVoxel0px1ny1pz() > 0 ? 1: 0;
			const VoxelType pVoxel0px0py1nz = volIter.peekVoxel0px0py1nz() > 0 ? 1: 0;
			//const VoxelType pVoxel0px0py0pz = volIter.peekVoxel0px0py0pz() > 0 ? 1: 0;
			const VoxelType pVoxel0px0py1pz = volIter.peekVoxel0px0py1pz() > 0 ? 1: 0;
			const VoxelType pVoxel0px1py1nz = volIter.peekVoxel0px1py1nz() > 0 ? 1: 0;
			const VoxelType pVoxel0px1py0pz = volIter.peekVoxel0px1py0pz() > 0 ? 1: 0;
			const VoxelType pVoxel0px1py1pz = volIter.peekVoxel0px1py1pz() > 0 ? 1: 0;

			const VoxelType pVoxel1px1ny1nz = volIter.peekVoxel1px1ny1nz() > 0 ? 1: 0;
			const VoxelType pVoxel1px1ny0pz = volIter.peekVoxel1px1ny0pz() > 0 ? 1: 0;
			const VoxelType pVoxel1px1ny1pz = volIter.peekVoxel1px1ny1pz() > 0 ? 1: 0;
			const VoxelType pVoxel1px0py1nz = volIter.peekVoxel1px0py1nz() > 0 ? 1: 0;
			const VoxelType pVoxel1px0py0pz = volIter.peekVoxel1px0py0pz() > 0 ? 1: 0;
			const VoxelType pVoxel1px0py1pz = volIter.peekVoxel1px0py1pz() > 0 ? 1: 0;
			const VoxelType pVoxel1px1py1nz = volIter.peekVoxel1px1py1nz() > 0 ? 1: 0;
			const VoxelType pVoxel1px1py0pz = volIter.peekVoxel1px1py0pz() > 0 ? 1: 0;
			const VoxelType pVoxel1px1py1pz = volIter.peekVoxel1px1py1pz() > 0 ? 1: 0;

			const int xGrad(- weights[0][0][0] * pVoxel1nx1ny1nz -
				weights[1][0][0] * pVoxel1nx1ny0pz - weights[2][0][0] *
				pVoxel1nx1ny1pz - weights[0][1][0] * pVoxel1nx0py1nz -
				weights[1][1][0] * pVoxel1nx0py0pz - weights[2][1][0] *
				pVoxel1nx0py1pz - weights[0][2][0] * pVoxel1nx1py1nz -
				weights[1][2][0] * pVoxel1nx1py0pz - weights[2][2][0] *
				pVoxel1nx1py1pz + weights[0][0][2] * pVoxel1px1ny1nz +
				weights[1][0][2] * pVoxel1px1ny0pz + weights[2][0][2] *
				pVoxel1px1ny1pz + weights[0][1][2] * pVoxel1px0py1nz +
				weights[1][1][2] * pVoxel1px0py0pz + weights[2][1][2] *
				pVoxel1px0py1pz + weights[0][2][2] * pVoxel1px1py1nz +
				weights[1][2][2] * pVoxel1px1py0pz + weights[2][2][2] *
				pVoxel1px1py1pz);

			const int yGrad(- weights[0][0][0] * pVoxel1nx1ny1nz -
				weights[1][0][0] * pVoxel1nx1ny0pz - weights[2][0][0] *
				pVoxel1nx1ny1pz + weights[0][2][0] * pVoxel1nx1py1nz +
				weights[1][2][0] * pVoxel1nx1py0pz + weights[2][2][0] *
				pVoxel1nx1py1pz - weights[0][0][1] * pVoxel0px1ny1nz -
				weights[1][0][1] * pVoxel0px1ny0pz - weights[2][0][1] *
				pVoxel0px1ny1pz + weights[0][2][1] * pVoxel0px1py1nz +
				weights[1][2][1] * pVoxel0px1py0pz + weights[2][2][1] *
				pVoxel0px1py1pz - weights[0][0][2] * pVoxel1px1ny1nz -
				weights[1][0][2] * pVoxel1px1ny0pz - weights[2][0][2] *
				pVoxel1px1ny1pz + weights[0][2][2] * pVoxel1px1py1nz +
				weights[1][2][2] * pVoxel1px1py0pz + weights[2][2][2] *
				pVoxel1px1py1pz);

			const int zGrad(- weights[0][0][0] * pVoxel1nx1ny1nz +
				weights[2][0][0] * pVoxel1nx1ny1pz - weights[0][1][0] *
				pVoxel1nx0py1nz + weights[2][1][0] * pVoxel1nx0py1pz -
				weights[0][2][0] * pVoxel1nx1py1nz + weights[2][2][0] *
				pVoxel1nx1py1pz - weights[0][0][1] * pVoxel0px1ny1nz +
				weights[2][0][1] * pVoxel0px1ny1pz - weights[0][1][1] *
				pVoxel0px0py1nz + weights[2][1][1] * pVoxel0px0py1pz -
				weights[0][2][1] * pVoxel0px1py1nz + weights[2][2][1] *
				pVoxel0px1py1pz - weights[0][0][2] * pVoxel1px1ny1nz +
				weights[2][0][2] * pVoxel1px1ny1pz - weights[0][1][2] *
				pVoxel1px0py1nz + weights[2][1][2] * pVoxel1px0py1pz -
				weights[0][2][2] * pVoxel1px1py1nz + weights[2][2][2] *
				pVoxel1px1py1pz);

			//Note: The above actually give gradients going from low density to high density.
			//For our normals we want the the other way around, so we switch the components as we return them.
			return Vector3DFloat(static_cast<float>(-xGrad),static_cast<float>(-yGrad),static_cast<float>(-zGrad));
	}
}
