class GC_LOSCell
{
	int m_iGX;
	int m_iGY;

	vector m_vCenter;
	vector m_vMin;
	vector m_vMax;

	GC_SightTraceResult m_losResult = GC_SightTraceResult.Processing;

	int m_iLastSeenFrame;
}

enum GC_SightTraceResult
{
	Processing,
	Terrain,
	ViewGeo,
	Free
}

class GC_LOSSystem : GameSystem
{
	SCR_MapEntity m_MapEntity;
	
	protected float m_fGridSize = 5;
	protected int m_iRadiusCells = 40;
	protected int m_iKeepRadiusCells = 100;

	protected int m_iFrame;

	protected ref map<string, ref GC_LOSCell> m_mCells = new map<string, ref GC_LOSCell>;
	protected ref array<ref GC_LOSCell> m_aTraceQueue = {};
	
	protected vector m_vStartPosition;
	
	protected float m_fSourceOffset = 1.5;
	protected float m_fTargetOffset = 0.75;
	
	static GC_LOSSystem GetInstance()
	{
		World world = GetGame().GetWorld();
		if (!world)
			return null;
		return GC_LOSSystem.Cast(world.FindSystem(GC_LOSSystem));
	}
	
	override static void InitInfo(WorldSystemInfo outInfo)
	{
		super.InitInfo(outInfo);
		outInfo
			.SetAbstract(false)
			.SetUnique(true)
			.SetLocation(WorldSystemLocation.Client)
	}
	
	protected void Init()
	{
		m_MapEntity = SCR_MapEntity.GetMapInstance();
		
		if (!m_MapEntity)
			return;
		
		m_MapEntity.GetOnMapOpen().Insert(OnMapOpen);
		m_MapEntity.GetOnMapClose().Insert(OnMapClose);
	}
	
	protected void OnMapOpen(MapConfiguration mapConfig)
	{
		SetState(true);
	}	
	
	protected void OnMapClose(MapConfiguration mapConfig)
	{
		SetState(false);
	}

	void UpdateAroundCursor(float cursorX, float cursorZ)
	{
		m_iFrame++;

		int cGX = WorldToGrid(cursorX);
		int cGZ = WorldToGrid(cursorZ);

		array<GC_LOSCell> cells = MarkWantedCells(cGX, cGZ, m_iRadiusCells);
		PruneOldCells(cGX, cGZ, m_iKeepRadiusCells);

		ProcessTraceQueue(500); // traces per update (tune)
		
		GC_LOSCheckerUI losMenu = GC_LOSCheckerUI.Cast(m_MapEntity.GetMapUIComponent(GC_LOSCheckerUI));
		losMenu.RegisterCells(cells);
		//PrintFormat("GC | losMenu: %1", losMenu);
	}
	
	protected array<GC_LOSCell> MarkWantedCells(int cGX, int cGZ, int radiusCells)
	{
		array<GC_LOSCell> cells = {};
		
		int rSq = radiusCells * radiusCells;

		for (int ix = -radiusCells; ix <= radiusCells; ix++)
		{
			for (int iz = -radiusCells; iz <= radiusCells; iz++)
			{
				if (ix * ix + iz * iz > rSq)
					continue;

				int gx = cGX + ix;
				int gy = cGZ + iz;

				string key = MakeKey(gx, gy);
				
				GC_LOSCell cell = m_mCells.Get(key);
				if (!cell)
				{
					cell = new GC_LOSCell();
					cell.m_iGX = gx;
					cell.m_iGY = gy;

					BuildCellBounds(cell);
					
					m_mCells.Set(key, cell);
					//PrintFormat("GC | Newcell: %1, %2 - Pos: %3", cell.m_iGX, cell.m_iGY, cell.m_vCenter);
					m_aTraceQueue.Insert(cell);
				}

				cells.Insert(cell);
				cell.m_iLastSeenFrame = m_iFrame;
			}
		}
		
		return cells;
	}
	
	protected void PruneOldCells(int cGX, int cGZ, int keepRadiusCells)
	{
		int keepSq = keepRadiusCells * keepRadiusCells;

		ref array<string> toRemove = {};

		foreach (string key, GC_LOSCell cell : m_mCells)
		{
			// If not recently seen, drop it
			if (cell.m_iLastSeenFrame != m_iFrame)
			{
				int dx = cell.m_iGX - cGX;
				int dz = cell.m_iGY - cGZ;

				if (dx * dx + dz * dz > keepSq)
					toRemove.Insert(key);
			}
		}

		foreach (string k : toRemove)
			m_mCells.Remove(k);
	}
	
	protected void ProcessTraceQueue(int maxTraces)
	{
		int count = m_aTraceQueue.Count();
			
		for (int i = count - 1; i >= Math.Max(count - maxTraces - 1, 0); i--)
		{
			GC_LOSCell cell = m_aTraceQueue[i];
			if (cell)
				DoLOSTrace(cell);
			m_aTraceQueue.Remove(i);
		}
	}

	protected void DoLOSTrace(GC_LOSCell cell)
	{
		if (!HasVisibility(cell.m_vCenter,TraceFlags.WORLD, EPhysicsLayerDefs.Terrain))
			cell.m_losResult = GC_SightTraceResult.Terrain;
		else if (!HasVisibility(cell.m_vCenter, TraceFlags.ENTS, EPhysicsLayerDefs.ViewGeometry))
			cell.m_losResult = GC_SightTraceResult.ViewGeo;
		else
			cell.m_losResult = GC_SightTraceResult.Free;
	}
	
	protected bool HasVisibility(vector toPos, TraceFlags flags, EPhysicsLayerDefs layer)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;
		
		TraceParam trace = new TraceParam();
		trace.Start = m_vStartPosition;
		trace.End = toPos;
		trace.Flags = flags;
		trace.TargetLayers = layer;
		
		float frac = world.TraceMove(trace);
		float dist = vector.Distance(trace.Start, trace.End);
		
		return (frac >= (dist - 1.0) / dist);
	}
	
	protected void BuildCellBounds(GC_LOSCell cell)
	{
		float minWorldX = (cell.m_iGX * m_fGridSize);
		float minWorldZ = (cell.m_iGY * m_fGridSize);
	
		float maxWorldX = minWorldX + m_fGridSize;
		float maxWorldZ = minWorldZ + m_fGridSize;
	
		cell.m_vMin = Vector(minWorldX, 0, minWorldZ);
		cell.m_vMax = Vector(maxWorldX, 0, maxWorldZ);
	
		cell.m_vCenter = Vector((minWorldX + maxWorldX) * 0.5, 0, (minWorldZ + maxWorldZ) * 0.5);
	
		// Cache ground height at the center
		cell.m_vCenter[1] = GetGame().GetWorld().GetSurfaceY(cell.m_vCenter[0], cell.m_vCenter[2]) + m_fTargetOffset;
	}
	
	protected int WorldToGrid(float world)
	{
		return Math.Floor(world / m_fGridSize);
	}

	protected float GridToWorld(int g)
	{
		return g * m_fGridSize;
	}

	protected string MakeKey(int gx, int gy)
	{
		return string.Format("%1:%2", gx, gy);
	}
	
	void SetStart(vector startPostion)
	{
		startPostion[1] = startPostion[1]  + m_fSourceOffset;
		
		m_vStartPosition = startPostion;
		
		m_mCells.Clear();
		m_aTraceQueue.Clear();
		m_MapEntity = SCR_MapEntity.GetMapInstance();
		Enable(true);
	}
	
	void SetState(bool enabled)
	{
		Enable(enabled);
		
		m_mCells.Clear();
	}
}