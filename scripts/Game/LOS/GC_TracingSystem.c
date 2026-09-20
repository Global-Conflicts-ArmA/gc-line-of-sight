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
	
	protected TextWidget m_wStatusWidget;

	
	protected int m_iMaintenanceBudget = 5000;
	protected int m_iSubdivCost = 50;
	
	protected vector m_vSourcePos;
	protected float m_fTargetOffset;
	
	protected vector m_vPreviousPan;
	protected float m_fPreviousZoom;
	protected bool m_bMapChanged;
	
	
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
	void ActivateTool(float worldX, float worldY, float sourceOffset, float targetOffset, TextWidget status)
	{
		DeactivateTool();
		
		Print("Activating tracing system");
		
		m_vSourcePos = Vector(worldX, Math.Max(0, GetGame().GetWorld().GetSurfaceY(worldX, worldY)) + sourceOffset, worldY);
		m_fTargetOffset = targetOffset;
		m_wStatusWidget = status;
		
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
		
		m_vPreviousPan = vector.Zero;
		m_fPreviousZoom = 0;
		
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
	
	//! Frame update event
	override void OnUpdate(WorldSystemPoint point)
	{
		super.OnUpdate(point);
		
		vector currentPan = m_MapEntity.GetCurrentPan();
		float currentZoom = m_MapEntity.GetCurrentZoom();
		
		
		bool mapChange = (m_vPreviousPan != currentPan) || (m_fPreviousZoom != currentZoom);
		
		if (mapChange)
			UpdateVerticesBulk();
		else if (m_bMapChanged)
			MaintainTree(true);
		else if (!m_NodeQueue.IsEmpty())
			MaintainTree(false);
		
		m_vPreviousPan = currentPan;
		m_fPreviousZoom = currentZoom;
		m_bMapChanged = mapChange;

		
		m_wStatusWidget.SetText("Q: " + m_NodeQueue.Count() + " A: " + m_aActiveNodes.Count());
	}
	
	//! Bulk process vertices instead of calling WorldToScreen individually
	void UpdateVerticesBulk()
	{
		const vector pan = m_MapEntity.GetCurrentPan();
		const float panX = pan[0];
		const float panY = pan[1];
	
		const vector offset = m_MapEntity.Offset();
		const float offsetX = offset[0];
		const float offsetY = offset[2] + m_MapEntity.GetMapSizeY();
	
		const float zoom = m_MapEntity.GetCurrentZoom();
	
		foreach (GC_QuadNode node : m_aActiveNodes)
		{
			const float x1 = (node.m_fX1 - offsetX) * zoom + panX;
			const float x2 = (node.m_fX2 - offsetX) * zoom + panX;
			const float y1 = (offsetY - node.m_fY1) * zoom + panY;
			const float y2 = (offsetY - node.m_fY2) * zoom + panY;
	
			node.m_DrawCommand.m_Vertices[0] = x2;
			node.m_DrawCommand.m_Vertices[1] = y2;
			node.m_DrawCommand.m_Vertices[2] = x1;
			node.m_DrawCommand.m_Vertices[3] = y2;
			node.m_DrawCommand.m_Vertices[4] = x1;
			node.m_DrawCommand.m_Vertices[5] = y1;
			node.m_DrawCommand.m_Vertices[6] = x2;
			node.m_DrawCommand.m_Vertices[7] = y1;
		}
	}
	
	//! Still cheaper than 4 WorldToScreen calls :)
	void UpdateVerticesSingle(GC_QuadNode node)
	{
		const vector pan = m_MapEntity.GetCurrentPan();
		const float panX = pan[0];
		const float panY = pan[1];
	
		const vector offset = m_MapEntity.Offset();
		const float offsetX = offset[0];
		const float offsetY = offset[2] + m_MapEntity.GetMapSizeY();
	
		const float zoom = m_MapEntity.GetCurrentZoom();
		
		const float x1 = (node.m_fX1 - offsetX) * zoom + panX;
		const float x2 = (node.m_fX2 - offsetX) * zoom + panX;
		const float y1 = (offsetY - node.m_fY1) * zoom + panY;
		const float y2 = (offsetY - node.m_fY2) * zoom + panY;
	
		node.m_DrawCommand.m_Vertices[0] = x2;
		node.m_DrawCommand.m_Vertices[1] = y2;
		node.m_DrawCommand.m_Vertices[2] = x1;
		node.m_DrawCommand.m_Vertices[3] = y2;
		node.m_DrawCommand.m_Vertices[4] = x1;
		node.m_DrawCommand.m_Vertices[5] = y1;
		node.m_DrawCommand.m_Vertices[6] = x2;
		node.m_DrawCommand.m_Vertices[7] = y1;
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
		
		const int intendedLevel = Math.Round(Math.Log(m_MapEntity.GetCurrentZoom()) * 1.44269504089 + 8); // 1.44269504089 = 1 / ln(2)
		
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
			
			if (!node.m_DrawCommand)
				node.CreateCommand();
			node.UpdateColor(m_bShadingMode);
			UpdateVerticesSingle(node);
			m_aDrawCommands.Insert(node.m_DrawCommand);
		}
	}
	
	/// performance improvement avenues:
	//  - vectorize additional operations / move them out of functions into loops
	//  - a lot of performance cost comes from simply moving draw command vertices around
	//		i could try not drawing clearly off-screen things, and i could try prioritizing removal of active nodes over addition to keep the amount low when moving the map
	//	- i could minimize the amount of active nodes by staying active until any children (or below) differ from self. this is probably a big improvement
	//	- i could stabilize trace cost by using an actual frame time / tick count budget rather than an arbitrary trace count (traces are not equal)
	
}