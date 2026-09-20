class GC_TracingSystem : GameSystem
{
	protected SCR_MapEntity m_MapEntity;
	
	protected CanvasWidget m_wCanvasWidget;
	
	//! Root node of quad tree
	protected ref GC_QuadNode m_QuadTree;
	
	//! Nodes that should have draw commands
	protected ref array<GC_QuadNode> m_aActiveNodes = {};
	
	//! Array of draw commands, rebuilt frequently
	protected ref array<ref CanvasWidgetCommand> m_aDrawCommands = {};

	//! Node queue
	protected ref GC_SimpleQueue<GC_QuadNode> m_NodeQueue = new GC_SimpleQueue<GC_QuadNode>();
	
	//! Shading mode
	protected GC_ShadingMode m_bShadingMode = GC_ShadingMode.Darken;

	
	protected int m_iMaintenanceBudget = 5000;
	protected int m_iSubdivCost = 50;
	
	protected vector m_vSourcePos;
	protected float m_fTargetOffset;
	
	
	override static void InitInfo(WorldSystemInfo outInfo)
	{
		super.InitInfo(outInfo);
		outInfo
			.SetAbstract(false)
			.SetUnique(true)
			.SetLocation(WorldSystemLocation.Client)
			.AddPoint(ESystemPoint.PostFrame);
	}
	
	//! Global init, even if the tool is not active yet
	override void OnInit()
	{
		super.OnInit();
		
		Enable(false);
		
		m_MapEntity = SCR_MapEntity.GetMapInstance();
	}
	
	//! Tool starts tracing
	void ActivateTool(float worldX, float worldY, float sourceOffset, float targetOffset)
	{
		DeactivateTool();
		
		Print("Activating tracing system");
		
		m_vSourcePos = Vector(worldX, Math.Max(0, GetGame().GetWorld().GetSurfaceY(worldX, worldY)) + sourceOffset, worldY);
		m_fTargetOffset = targetOffset;
		
		Init();
		
		
		Enable(true);
	}
	
	//! Tool stops tracing
	void DeactivateTool()
	{
		Print("Deactivating tracing system");
		
		if (m_wCanvasWidget)
   			 m_wCanvasWidget.RemoveFromHierarchy();
		
		m_NodeQueue.Clear();
		m_QuadTree = null;
		m_aActiveNodes.Clear();
		m_wCanvasWidget = null;
		m_aDrawCommands.Clear();
		
		Enable(false);
	}
	
	//! Tool init (tool is opened etc)
	protected void Init()
	{
		m_bMapChanged = true;
		
		vector offset = m_MapEntity.Offset();
		vector size = m_MapEntity.Size();
		m_QuadTree = new GC_QuadNode(m_vSourcePos, m_fTargetOffset, 0, offset[0], offset[0] + size[0], offset[2], offset[2] + size[2]);
		ActivateNode(m_QuadTree);
		
		Widget mapFrame = m_MapEntity.GetMapMenuRoot().FindAnyWidget(SCR_MapConstants.MAP_FRAME_NAME);
		if (!mapFrame)
			mapFrame = m_MapEntity.GetMapMenuRoot();
		if (!mapFrame)
			 return;
		
		m_wCanvasWidget = CanvasWidget.Cast(GetGame().GetWorkspace().CreateWidgets("{F928661E727CC638}UI/Map/GC_LOSCanvas.layout", mapFrame));
		m_wCanvasWidget.SetDrawCommands(m_aDrawCommands);
	}
	
	protected vector m_vPreviousPan;
	protected float m_fPreviousZoom;
	protected bool m_bMapChanged;
	
	//! Frame update event
	override void OnUpdate(WorldSystemPoint point)
	{
		super.OnUpdate(point);
		
		vector currentPan = m_MapEntity.GetCurrentPan();
		float currentZoom = m_MapEntity.GetCurrentZoom();
		
		bool mapChange = (m_vPreviousPan != currentPan) || (m_fPreviousZoom != currentZoom);
		
		if (mapChange)
			foreach (GC_QuadNode node : m_aActiveNodes)
				node.UpdateVertices(m_MapEntity);
		else if (m_bMapChanged)
			MaintainTree(true);
		else if (!m_NodeQueue.IsEmpty())
			MaintainTree(false);
		
		m_vPreviousPan = currentPan;
		m_fPreviousZoom = currentZoom;
		m_bMapChanged = mapChange;

	}
		
		// tree states:
		// - no map change, queue empty => do nothing
		// - no map change, queue not empty => keep working on it (and update new or all commands)
		// - map change => clear node queue (?) then do maintenance, finally update all commands
		
		// should we clear the queue on map change? possibilities:
		// - clear it on map change then immediately restart maintenance
		// - clear it but wait until post change to restart maintenance
		// - clear it only after map change
		// this is also relevant when the queue is empty, when do we restart maintenance in general?
		// - only after map move
		// - on map move
	
	
	void SetShadingMode(GC_ShadingMode mode)
	{
		if (m_bShadingMode != mode)
		{
			m_bShadingMode = mode;
			foreach (GC_QuadNode node : m_aActiveNodes)
				node.UpdateColor(m_bShadingMode);
		}
	}
	
	//! Maintains the quad tree, removing obsolete nodes and adding required nodes.
	protected void MaintainTree(bool restart)
	{
		
		const int intendedLevel = Math.Round(Math.Log2(m_MapEntity.GetCurrentZoom()) * 1.44269504089 + 8); // 1.44269504089 = 1 / ln(2)
		
		// zoom is max 20
		// for every doubling in zoom, level should increase by 1
		
		// level 8 might be about right for zoom 1
		// 0.25 => 6
		// 0.5 => 7
		// 1 => 8
		// 2 => 9
		// 4 => 10
		// 8 => 11
		// 16 => 12
		
		
		// okay so if i start the tool everything is normal
		// but if i then move around the map, it explodes (no restart happens yet)
		
		// could try returning here after initial generation, to confirm the issue originates here
		
		if (restart)
		{
			Print(restart);
			m_NodeQueue.Clear();
			m_NodeQueue.Enqueue(m_QuadTree);
		}
		
		
		int frameCost = 0;
		
		vector frameMin, frameMax;
		m_MapEntity.GetMapVisibleFrame(frameMin, frameMax);
		const float frameX1 = frameMin[0];
		const float frameX2 = frameMax[0];
		const float frameY1 = frameMin[2];
		const float frameY2 = frameMax[2];

		while (frameCost < m_iMaintenanceBudget)
		{
			GC_QuadNode node = m_NodeQueue.Deque();
			if (!node)
				break; // done
			
			frameCost += 1;
			const bool levelReached = node.m_iLevel >= intendedLevel;
			const bool intersection = GC_TracingHelper.BboxIntersects(frameX1, frameX2, frameY1, frameY2, node.m_fX1, node.m_fX2, node.m_fY1, node.m_fY2);
			const bool hasChildren = node.m_Q1 != null;
			
			
			if (levelReached || !intersection)
			{
				// null + deactivate children, make self active
				if (hasChildren)
				{
					DeactivateChildren(node);
					node.m_Q1 = null;
					node.m_Q2 = null;
					node.m_Q3 = null;
					node.m_Q4 = null;
					ActivateNode(node);
				}
			}
			else
			{
				if (!hasChildren)
				{
					// create and activate children, deactivate self
					frameCost += m_iSubdivCost;
					
					float midX = node.m_fX1 + (node.m_fX2 - node.m_fX1) / 2;
					float midY = node.m_fY1 + (node.m_fY2 - node.m_fY1) / 2;
					
					//  II   I
					// III  IV
					
					node.m_Q1 = new GC_QuadNode(m_vSourcePos, m_fTargetOffset, node.m_iLevel + 1, midX, node.m_fX2, midY, node.m_fY2);
					node.m_Q2 = new GC_QuadNode(m_vSourcePos, m_fTargetOffset, node.m_iLevel + 1, node.m_fX1, midX, midY, node.m_fY2);
					node.m_Q3 = new GC_QuadNode(m_vSourcePos, m_fTargetOffset, node.m_iLevel + 1, node.m_fX1, midX, node.m_fY1, midY);
					node.m_Q4 = new GC_QuadNode(m_vSourcePos, m_fTargetOffset, node.m_iLevel + 1, midX, node.m_fX2, node.m_fY1, midY);
					
					DeactivateNode(node);
					ActivateNode(node.m_Q1);
					ActivateNode(node.m_Q2);
					ActivateNode(node.m_Q3);
					ActivateNode(node.m_Q4);
				}
				// enqueue children
				m_NodeQueue.Enqueue(node.m_Q1);
				m_NodeQueue.Enqueue(node.m_Q2);
				m_NodeQueue.Enqueue(node.m_Q3);
				m_NodeQueue.Enqueue(node.m_Q4);
			}
			
		}
		
		// not done probably
			
	}
	
	void DeactivateChildren(GC_QuadNode node)
	{
		SCR_Stack<GC_QuadNode> nodeStack = new SCR_Stack<GC_QuadNode>;
		nodeStack.Push(node.m_Q1);
		nodeStack.Push(node.m_Q2);
		nodeStack.Push(node.m_Q3);
		nodeStack.Push(node.m_Q4);
		
		while (!nodeStack.IsEmpty())
		{
			node = nodeStack.Pop();
			if (node.m_Q1)
			{
				nodeStack.Push(node.m_Q1);
				nodeStack.Push(node.m_Q2);
				nodeStack.Push(node.m_Q3);
				nodeStack.Push(node.m_Q4);
			}
			if (node.m_iActiveIndex >= 0)
				DeactivateNode(node);
		}
	}

	void DeactivateNode(GC_QuadNode node)
	{
		int index = node.m_iActiveIndex;
		if (index >= 0)
		{
			node.m_iActiveIndex = -1;
			m_aDrawCommands.Remove(index);
			m_aActiveNodes.Remove(index);
			if (index < m_aActiveNodes.Count())
				m_aActiveNodes[index].m_iActiveIndex = index;
		}
	}

	void ActivateNode(GC_QuadNode node)
	{
		if (node.m_iActiveIndex < 0) // this if might be redundant
		{
			m_aActiveNodes.Insert(node);
			node.m_iActiveIndex = m_aActiveNodes.Count() - 1;
			
			node.CreateCommand(m_bShadingMode);
			node.UpdateVertices(m_MapEntity);
			m_aDrawCommands.Insert(node.m_DrawCommand);
		}
	}

}