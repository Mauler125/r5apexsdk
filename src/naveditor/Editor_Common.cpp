//
// Copyright (c) 2009-2010 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include "Shared/Include/SharedAssert.h"
#include "Include/Editor.h"
#include "Include/Editor_Common.h"
#include "DebugUtils/Include/RecastDebugDraw.h"
#include "DebugUtils/Include/DetourDebugDraw.h"
#include "Include/InputGeom.h"
#include <DetourTileCache/Include/DetourTileCache.h>

// todo(amos): move these to common math.
inline unsigned int nextPow2(unsigned int v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

// todo(amos): move these to common math.
inline unsigned int ilog2(unsigned int v)
{
	unsigned int r;
	unsigned int shift;
	r = (v > 0xffff) << 4; v >>= r;
	shift = (v > 0xff) << 3; v >>= shift; r |= shift;
	shift = (v > 0xf) << 2; v >>= shift; r |= shift;
	shift = (v > 0x3) << 1; v >>= shift; r |= shift;
	r |= (v >> 1);
	return r;
}

static void EditorCommon_DrawInputGeometry(duDebugDraw* const dd, const InputGeom* const geom,
	const float maxSlope, const float textureScale)
{
	duDebugDrawTriMeshSlope(dd, geom->getMesh()->getVerts(), geom->getMesh()->getVertCount(),
		geom->getMesh()->getTris(), geom->getMesh()->getNormals(), geom->getMesh()->getTriCount(),
		maxSlope, textureScale);
}

static void EditorCommon_DrawBoundingBox(duDebugDraw* const dd, const InputGeom* const geom)
{
	// Draw bounds
	const float* const bmin = geom->getNavMeshBoundsMin();
	const float* const bmax = geom->getNavMeshBoundsMax();
	duDebugDrawBoxWire(dd, bmin[0], bmin[1], bmin[2], bmax[0], bmax[1], bmax[2], duRGBA(255, 255, 255, 128), 1.0f);
}

static void EditorCommon_DrawTilingGrid(duDebugDraw* const dd, const InputGeom* const geom, const int tileSize, const float cellSize)
{
	const float* const bmin = geom->getNavMeshBoundsMin();
	const float* const bmax = geom->getNavMeshBoundsMax();

	int gw = 0, gh = 0;
	rcCalcGridSize(bmin, bmax, cellSize, &gw, &gh);

	const int tw = (gw + tileSize - 1) / tileSize;
	const int th = (gh + tileSize - 1) / tileSize;
	const float s = tileSize * cellSize;

	duDebugDrawGridXY(dd, bmax[0], bmin[1], bmin[2], tw, th, s, duRGBA(0, 0, 0, 64), 1.0f);
}

int EditorCommon_SetAndRenderTileProperties(const InputGeom* const geom, const int tileSize,
	const float cellSize, int& maxTiles, int& maxPolysPerTile)
{
	int gridSize = 1;

	if (geom)
	{
		int gw = 0, gh = 0;
		const float* bmin = geom->getNavMeshBoundsMin();
		const float* bmax = geom->getNavMeshBoundsMax();
		rcCalcGridSize(bmin, bmax, cellSize, &gw, &gh);
		const int ts = tileSize;
		const int tw = (gw + ts-1) / ts;
		const int th = (gh + ts-1) / ts;

		ImGui::Text("Tiles: %d x %d", tw, th);
		ImGui::Text("Tile Sizes: %g x %g (%g)", tw* cellSize, th*cellSize, tileSize*cellSize);

		// Max tiles and max polys affect how the tile IDs are calculated.
		// There are 28 bits available for identifying a tile and a polygon.
		int tileBits = rcMin((int)ilog2(nextPow2(tw*th)), 16);
		int polyBits = 28 - tileBits;

		maxTiles = 1 << tileBits;
		maxPolysPerTile = 1 << polyBits;

		gridSize = tw*th;

		ImGui::Text("Max Tiles: %d", maxTiles);
		ImGui::Text("Max Polys: %d", maxPolysPerTile);
	}
	else
	{
		maxTiles = 0;
		maxPolysPerTile = 0;
		gridSize = 1;
	}

	return gridSize;
}

Editor_StaticTileMeshCommon::Editor_StaticTileMeshCommon()
	: m_triareas(nullptr)
	, m_solid(nullptr)
	, m_chf(nullptr)
	, m_cset(nullptr)
	, m_pmesh(nullptr)
	, m_dmesh(nullptr)
	, m_tileMeshDrawFlags(TM_DRAWFLAGS_INPUT_MESH|TM_DRAWFLAGS_NAVMESH)
	, m_tileCol(duRGBA(0, 0, 0, 32))
	, m_totalBuildTimeMs(0.0f)
	, m_drawActiveTile(false)
	, m_keepInterResults(false)
{
	m_lastBuiltTileBmin[0] = 0.0f;
	m_lastBuiltTileBmin[1] = 0.0f;
	m_lastBuiltTileBmin[2] = 0.0f;

	m_lastBuiltTileBmax[0] = 0.0f;
	m_lastBuiltTileBmax[1] = 0.0f;
	m_lastBuiltTileBmax[2] = 0.0f;

	memset(&m_cfg, 0, sizeof(rcConfig));
}

void Editor_StaticTileMeshCommon::cleanup()
{
	delete[] m_triareas;
	m_triareas = 0;
	rcFreeHeightField(m_solid);
	m_solid = 0;
	rcFreeCompactHeightfield(m_chf);
	m_chf = 0;
	rcFreeContourSet(m_cset);
	m_cset = 0;
	rcFreePolyMesh(m_pmesh);
	m_pmesh = 0;
	rcFreePolyMeshDetail(m_dmesh);
	m_dmesh = 0;
}

void Editor_StaticTileMeshCommon::renderTileMeshRenderOptions()
{
	ImGui::Text("TileMesh Render Options");

	bool isEnabled = m_tileMeshDrawFlags & TM_DRAWFLAGS_INPUT_MESH;

	// This should always be available, since if we load a large mesh we want to
	// be able to toggle this off to save on performance. The renderer has to be
	// moved to its own thread to solve this issue.
	if (ImGui::Checkbox("Input Mesh", &isEnabled))
		toggleTileMeshDrawFlag(TM_DRAWFLAGS_INPUT_MESH);

	// Check which modes are valid.
	//const bool hasNavMesh =m_navMesh != 0;
	const bool hasChf = m_chf != 0;
	const bool hasCset = m_cset != 0;
	const bool hasSolid = m_solid != 0;
	const bool hasDMesh = m_dmesh != 0;

	const bool intermediateDataUnavailable = !hasChf || !hasCset || !hasSolid || !hasDMesh;

	isEnabled = getTileMeshDrawFlags() & TM_DRAWFLAGS_NAVMESH;
	//ImGui::BeginDisabled(!hasNavMesh);

	if (ImGui::Checkbox("NavMesh", &isEnabled))
		toggleTileMeshDrawFlag(TM_DRAWFLAGS_NAVMESH);

	//ImGui::EndDisabled();

	isEnabled = getTileMeshDrawFlags() & TM_DRAWFLAGS_VOXELS;
	ImGui::BeginDisabled(!hasSolid);

	if (ImGui::Checkbox("Voxels", &isEnabled))
		toggleTileMeshDrawFlag(TM_DRAWFLAGS_VOXELS);

	ImGui::EndDisabled();

	isEnabled = getTileMeshDrawFlags() & TM_DRAWFLAGS_VOXELS_WALKABLE;
	ImGui::BeginDisabled(!hasSolid);

	if (ImGui::Checkbox("Walkable Voxels", &isEnabled))
		toggleTileMeshDrawFlag(TM_DRAWFLAGS_VOXELS_WALKABLE);

	ImGui::EndDisabled();

	isEnabled = getTileMeshDrawFlags() & TM_DRAWFLAGS_COMPACT;
	ImGui::BeginDisabled(!hasChf);

	if (ImGui::Checkbox("Compact", &isEnabled))
		toggleTileMeshDrawFlag(TM_DRAWFLAGS_COMPACT);

	ImGui::EndDisabled();

	isEnabled = getTileMeshDrawFlags() & TM_DRAWFLAGS_COMPACT_DISTANCE;
	ImGui::BeginDisabled(!hasChf);

	if (ImGui::Checkbox("Compact Distance", &isEnabled))
		toggleTileMeshDrawFlag(TM_DRAWFLAGS_COMPACT_DISTANCE);

	ImGui::EndDisabled();

	isEnabled = getTileMeshDrawFlags() & TM_DRAWFLAGS_COMPACT_REGIONS;
	ImGui::BeginDisabled(!hasChf);

	if (ImGui::Checkbox("Compact Regions", &isEnabled))
		toggleTileMeshDrawFlag(TM_DRAWFLAGS_COMPACT_REGIONS);

	ImGui::EndDisabled();

	isEnabled = getTileMeshDrawFlags() & TM_DRAWFLAGS_REGION_CONNECTIONS;
	ImGui::BeginDisabled(!hasCset);

	if (ImGui::Checkbox("Region Connections", &isEnabled))
		toggleTileMeshDrawFlag(TM_DRAWFLAGS_REGION_CONNECTIONS);

	ImGui::EndDisabled();

	isEnabled = getTileMeshDrawFlags() & TM_DRAWFLAGS_RAW_CONTOURS;
	ImGui::BeginDisabled(!hasCset);

	if (ImGui::Checkbox("Raw Contours", &isEnabled))
		toggleTileMeshDrawFlag(TM_DRAWFLAGS_RAW_CONTOURS);

	ImGui::EndDisabled();

	isEnabled = getTileMeshDrawFlags() & TM_DRAWFLAGS_CONTOURS;
	ImGui::BeginDisabled(!hasCset);

	if (ImGui::Checkbox("Contours", &isEnabled))
		toggleTileMeshDrawFlag(TM_DRAWFLAGS_CONTOURS);

	ImGui::EndDisabled();

	isEnabled = getTileMeshDrawFlags() & TM_DRAWFLAGS_POLYMESH;
	ImGui::BeginDisabled(!hasDMesh);

	if (ImGui::Checkbox("Poly Mesh", &isEnabled))
		toggleTileMeshDrawFlag(TM_DRAWFLAGS_POLYMESH);

	ImGui::EndDisabled();

	isEnabled = getTileMeshDrawFlags() & TM_DRAWFLAGS_POLYMESH_DETAIL;
	ImGui::BeginDisabled(!hasDMesh);

	if (ImGui::Checkbox("Poly Mesh Detail", &isEnabled))
		toggleTileMeshDrawFlag(TM_DRAWFLAGS_POLYMESH_DETAIL);

	ImGui::EndDisabled();

	if (intermediateDataUnavailable)
	{
		ImGui::Separator();

		ImGui::Text("Tick 'Keep Intermediate Results'");
		ImGui::Text("rebuild some tiles to see");
		ImGui::Text("more debug mode options.");
	}
}

void Editor_StaticTileMeshCommon::renderTileMeshData()
{
	if (!m_geom || !m_geom->getMesh())
		return;

	const float texScale = 1.0f / (m_cellSize * 10.0f);

	// Draw input mesh
	if (getTileMeshDrawFlags() & TM_DRAWFLAGS_INPUT_MESH)
		EditorCommon_DrawInputGeometry(&m_dd, m_geom, m_agentMaxSlope, texScale);

	glDepthMask(GL_FALSE);

	// Draw bounds
	EditorCommon_DrawBoundingBox(&m_dd, m_geom);

	// Tiling grid.
	EditorCommon_DrawTilingGrid(&m_dd, m_geom, m_tileSize, m_cellSize);

	if (m_drawActiveTile)
	{
		// Draw active tile
		duDebugDrawBoxWire(&m_dd, m_lastBuiltTileBmin[0], m_lastBuiltTileBmin[1], m_lastBuiltTileBmin[2],
			m_lastBuiltTileBmax[0], m_lastBuiltTileBmax[1], m_lastBuiltTileBmax[2], m_tileCol, 1.0f);
	}

	if (m_navMesh && m_navQuery)
	{
		if (m_tileMeshDrawFlags & TM_DRAWFLAGS_NAVMESH)
		{
			duDebugDrawNavMeshWithClosedList(&m_dd, *m_navMesh, *m_navQuery, m_navMeshDrawFlags);
			duDebugDrawNavMeshPolysWithFlags(&m_dd, *m_navMesh, EDITOR_POLYFLAGS_DISABLED, duRGBA(0, 0, 0, 128));
		}
	}

	glDepthMask(GL_TRUE);

	if (m_chf)
	{
		if (getTileMeshDrawFlags() & TM_DRAWFLAGS_COMPACT)
			duDebugDrawCompactHeightfieldSolid(&m_dd, *m_chf);

		if (getTileMeshDrawFlags() & TM_DRAWFLAGS_COMPACT_DISTANCE)
			duDebugDrawCompactHeightfieldDistance(&m_dd, *m_chf);
		if (getTileMeshDrawFlags() & TM_DRAWFLAGS_COMPACT_REGIONS)
			duDebugDrawCompactHeightfieldRegions(&m_dd, *m_chf);
	}

	if (m_solid)
	{
		if (getTileMeshDrawFlags() & TM_DRAWFLAGS_VOXELS)
		{
			glEnable(GL_FOG);
			duDebugDrawHeightfieldSolid(&m_dd, *m_solid);
			glDisable(GL_FOG);
		}
		if (getTileMeshDrawFlags() & TM_DRAWFLAGS_VOXELS_WALKABLE)
		{
			glEnable(GL_FOG);
			duDebugDrawHeightfieldWalkable(&m_dd, *m_solid);
			glDisable(GL_FOG);
		}
	}

	if (m_cset)
	{
		if (getTileMeshDrawFlags() & TM_DRAWFLAGS_RAW_CONTOURS)
		{
			glDepthMask(GL_FALSE);
			duDebugDrawRawContours(&m_dd, *m_cset);
			glDepthMask(GL_TRUE);
		}
		if (getTileMeshDrawFlags() & TM_DRAWFLAGS_CONTOURS)
		{
			glDepthMask(GL_FALSE);
			duDebugDrawContours(&m_dd, *m_cset);
			glDepthMask(GL_TRUE);
		}
	}

	if ((m_chf &&m_cset) && 
		(getTileMeshDrawFlags() & TM_DRAWFLAGS_REGION_CONNECTIONS))
	{
		duDebugDrawCompactHeightfieldRegions(&m_dd, *m_chf);

		glDepthMask(GL_FALSE);
		duDebugDrawRegionConnections(&m_dd, *m_cset);
		glDepthMask(GL_TRUE);
	}

	if (m_pmesh && (getTileMeshDrawFlags() & TM_DRAWFLAGS_POLYMESH))
	{
		glDepthMask(GL_FALSE);
		duDebugDrawPolyMesh(&m_dd, *m_pmesh);
		glDepthMask(GL_TRUE);
	}

	if (m_dmesh && (getTileMeshDrawFlags() & TM_DRAWFLAGS_POLYMESH_DETAIL))
	{
		glDepthMask(GL_FALSE);
		duDebugDrawPolyMeshDetail(&m_dd, *m_dmesh);
		glDepthMask(GL_TRUE);
	}

	// TODO: also add flags for this
	m_geom->drawConvexVolumes(&m_dd);
	m_geom->drawOffMeshConnections(&m_dd);

	if (m_tool)
		m_tool->handleRender();

	renderToolStates();

	glDepthMask(GL_TRUE);
}

void Editor_StaticTileMeshCommon::renderIntermediateTileMeshOptions()
{
	ImGui::Indent();
	ImGui::Indent();

	if (ImGui::Button("Load", ImVec2(123, 0)))
	{
		dtFreeNavMesh(m_navMesh);
		m_navMesh = Editor::loadAll(m_modelName.c_str());
		m_navQuery->init(m_navMesh, 2048);

		m_loadedNavMeshType = m_selectedNavMeshType;
		initToolStates(this);
	}

	if (ImGui::Button("Save", ImVec2(123, 0)))
	{
		Editor::saveAll(m_modelName.c_str(), m_navMesh);
	}

	ImGui::Unindent();
	ImGui::Unindent();

	ImGui::Text("Build Time: %.1fms", m_totalBuildTimeMs);

	if (m_navMesh)
	{
		const dtNavMeshParams& params = m_navMesh->m_params;
		const float* origin = m_navMesh->m_orig;

		ImGui::Text("Mesh Origin: \n\tX: %g \n\tY: %g \n\tZ: %g", origin[0], origin[1], origin[2]);
		ImGui::Text("Tile Dimensions: %g x %g", params.tileWidth, params.tileHeight);
		ImGui::Text("Poly Group Count: %d", params.polyGroupCount);
		ImGui::Text("Traversal Table Size: %d", params.traversalTableSize);
		ImGui::Text("Traversal Table Count: %d", params.traversalTableCount);
		ImGui::Text("Max Tiles: %d", params.maxTiles);
		ImGui::Text("Max Polys: %d", params.maxPolys);

		ImGui::Separator();
	}
	else
		ImGui::Separator();
}

void drawTiles(duDebugDraw* dd, dtTileCache* tc)
{
	unsigned int fcol[6];
	float bmin[3], bmax[3];

	for (int i = 0; i < tc->getTileCount(); ++i)
	{
		const dtCompressedTile* tile = tc->getTile(i);
		if (!tile->header) continue;

		tc->calcTightTileBounds(tile->header, bmin, bmax);

		const unsigned int col = duIntToCol(i, 64);
		duCalcBoxColors(fcol, col, col);
		duDebugDrawBox(dd, bmin[0], bmin[1], bmin[2], bmax[0], bmax[1], bmax[2], fcol);
	}

	for (int i = 0; i < tc->getTileCount(); ++i)
	{
		const dtCompressedTile* tile = tc->getTile(i);
		if (!tile->header) continue;

		tc->calcTightTileBounds(tile->header, bmin, bmax);

		const unsigned int col = duIntToCol(i, 255);
		const float pad = tc->getParams()->cs * 0.1f;
		duDebugDrawBoxWire(dd, bmin[0] - pad, bmin[1] - pad, bmin[2] - pad,
			bmax[0] + pad, bmax[1] + pad, bmax[2] + pad, col, 2.0f);
	}
}

void drawObstacles(duDebugDraw* dd, const dtTileCache* tc)
{
	// Draw obstacles
	for (int i = 0; i < tc->getObstacleCount(); ++i)
	{
		const dtTileCacheObstacle* ob = tc->getObstacle(i);
		if (ob->state == DT_OBSTACLE_EMPTY) continue;
		float bmin[3], bmax[3];
		tc->getObstacleBounds(ob, bmin, bmax);

		unsigned int col = 0;
		if (ob->state == DT_OBSTACLE_PROCESSING)
			col = duRGBA(255, 255, 0, 128);
		else if (ob->state == DT_OBSTACLE_PROCESSED)
			col = duRGBA(255, 192, 0, 192);
		else if (ob->state == DT_OBSTACLE_REMOVING)
			col = duRGBA(220, 0, 0, 128);

		duDebugDrawCylinder(dd, bmin[0], bmin[1], bmin[2], bmax[0], bmax[1], bmax[2], col);
		duDebugDrawCylinderWire(dd, bmin[0], bmin[1], bmin[2], bmax[0], bmax[1], bmax[2], duDarkenCol(col), 2);
	}
}

Editor_DynamicTileMeshCommon::Editor_DynamicTileMeshCommon()
	: m_tileCache(nullptr)
	, m_talloc(nullptr)
	, m_tcomp(nullptr)
	, m_tmproc(nullptr)
	, m_cacheBuildTimeMs(0.0f)
	, m_cacheCompressedSize(0)
	, m_cacheRawSize(0)
	, m_cacheLayerCount(0)
	, m_cacheBuildMemUsage(0)
	, m_tileMeshDrawFlags(TM_DRAWFLAGS_INPUT_MESH|TM_DRAWFLAGS_NAVMESH)
	, m_keepInterResults(false)
{
}

void Editor_DynamicTileMeshCommon::renderTileMeshRenderOptions()
{
	ImGui::Text("TileMesh Render Options");

	bool isEnabled = m_tileMeshDrawFlags & TM_DRAWFLAGS_INPUT_MESH;

	// This should always be available, since if we load a large mesh we want to
	// be able to toggle this off to save on performance. The renderer has to be
	// moved to its own thread to solve this issue.
	if (ImGui::Checkbox("Input Mesh", &isEnabled))
		toggleTileMeshDrawFlag(TM_DRAWFLAGS_INPUT_MESH);

	isEnabled = getTileMeshDrawFlags() & TM_DRAWFLAGS_NAVMESH;
	//ImGui::BeginDisabled(!hasNavMesh);

	if (ImGui::Checkbox("NavMesh", &isEnabled))
		toggleTileMeshDrawFlag(TM_DRAWFLAGS_NAVMESH);

	//ImGui::EndDisabled();

	isEnabled = getTileMeshDrawFlags() & TM_DRAWFLAGS_TILE_CACHE_BOUNDS;

	ImGui::BeginDisabled(!m_tileCache);

	if (ImGui::Checkbox("Cache Bounds", &isEnabled))
		toggleTileMeshDrawFlag(TM_DRAWFLAGS_TILE_CACHE_BOUNDS);

	isEnabled = getTileMeshDrawFlags() & TM_DRAWFLAGS_TILE_CACHE_OBSTACLES;

	if (ImGui::Checkbox("Temp Obstacles", &isEnabled))
		toggleTileMeshDrawFlag(TM_DRAWFLAGS_TILE_CACHE_OBSTACLES);

	ImGui::EndDisabled();
}

void Editor_DynamicTileMeshCommon::renderTileMeshData()
{
	if (!m_geom || !m_geom->getMesh())
		return;

	const float texScale = 1.0f / (m_cellSize * 10.0f);
	const unsigned int drawFlags = getTileMeshDrawFlags();

	// Draw input mesh
	if (getTileMeshDrawFlags() & TM_DRAWFLAGS_INPUT_MESH)
		EditorCommon_DrawInputGeometry(&m_dd, m_geom, m_agentMaxSlope, texScale);

	// Draw bounds
	EditorCommon_DrawBoundingBox(&m_dd, m_geom);

	// Tiling grid.
	EditorCommon_DrawTilingGrid(&m_dd, m_geom, m_tileSize, m_cellSize);

	if (m_tileCache && drawFlags & TM_DRAWFLAGS_TILE_CACHE_BOUNDS)
		drawTiles(&m_dd, m_tileCache);

	if (m_tileCache && drawFlags & TM_DRAWFLAGS_TILE_CACHE_OBSTACLES)
		drawObstacles(&m_dd, m_tileCache);

	const bool navMeshRenderingEnabled = (drawFlags & TM_DRAWFLAGS_NAVMESH) != 0;

	if (m_navMesh && m_navQuery)
	{
		if (drawFlags & TM_DRAWFLAGS_NAVMESH)
		{
			duDebugDrawNavMeshWithClosedList(&m_dd, *m_navMesh, *m_navQuery, m_navMeshDrawFlags);
			duDebugDrawNavMeshPolysWithFlags(&m_dd, *m_navMesh, EDITOR_POLYFLAGS_DISABLED, duRGBA(0, 0, 0, 128));
		}
	}

	// TODO: also add flags for this
	m_geom->drawConvexVolumes(&m_dd);
	m_geom->drawOffMeshConnections(&m_dd);

	if (m_tool)
		m_tool->handleRender();

	renderToolStates();

	glDepthMask(GL_TRUE);
}