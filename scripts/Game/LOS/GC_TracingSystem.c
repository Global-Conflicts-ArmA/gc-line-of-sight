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
	
	//! Map item that highlights the current source position
	protected ref MapItem m_SourceMarker;
	
	protected TextWidget m_wStatusWidget;

	//! For how many ms the system may trace per frame. Increases CPU load but speeds up subdivision.
	protected const int m_iTickBudget = 5;
	
	protected const int m_iQuadWidth = 150;
	
	protected vector m_vSourcePos;
	protected float m_fTargetOffset;
	
	protected vector m_vPreviousPan;
	protected float m_fPreviousZoom;
	protected bool m_bRestartScheduled;
	
	protected float m_fResolutionMultiplier = 1;
	
	
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
		
		// Set positions
		m_vSourcePos = Vector(worldX, Math.Max(0, GetGame().GetWorld().GetSurfaceY(worldX, worldY)) + sourceOffset, worldY);
		m_fTargetOffset = targetOffset;
		m_wStatusWidget = status;
		
		// Source map item
		m_SourceMarker = m_MapEntity.CreateCustomMapItem();
		m_SourceMarker.SetPos(worldX, worldY);
		m_SourceMarker.SetBaseType(EMapDescriptorType.MDT_VIEWPOINT);
		m_SourceMarker.SetImageDef("view-point");
		MapDescriptorProps props = m_SourceMarker.GetProps();
		props.SetFrontColor(Color.FromInt(Color.BLACK));
		props.SetBackgroundColor(Color.FromInt(Color.BLACK));
		props.SetIconSize(1, 0.25, 4);
		props.Activate(true);
		m_SourceMarker.SetProps(props);
		
		// Init quadtree
		vector offset = m_MapEntity.Offset();
		vector size = m_MapEntity.Size();
		m_QuadTree = new GC_QuadNode(null, m_vSourcePos, m_fTargetOffset, 0, offset[0], offset[0] + size[0], offset[2], offset[2] + size[2]);
		ActivateNode(m_QuadTree);
		
		// Canvas widget
		Widget mapFrame = m_MapEntity.GetMapMenuRoot().FindAnyWidget(SCR_MapConstants.MAP_FRAME_NAME);
		if (!mapFrame)
			mapFrame = m_MapEntity.GetMapMenuRoot();
		if (!mapFrame)
			 return;
		m_wCanvasWidget = CanvasWidget.Cast(GetGame().GetWorkspace().CreateWidgets("{F928661E727CC638}UI/Map/GC_LOSCanvas.layout", mapFrame));
		m_wCanvasWidget.SetDrawCommands(m_aDrawCommands);
		
		m_bRestartScheduled = true;
		
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
		
		if (m_SourceMarker)
			m_SourceMarker.Recycle();
		
		Enable(false);
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
		else if (m_bRestartScheduled)
			MaintainTree(true);
		else if (!m_NodeQueue.IsEmpty())
			MaintainTree(false);
		
		m_vPreviousPan = currentPan;
		m_fPreviousZoom = currentZoom;
		m_bRestartScheduled = mapChange;

		
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
		
		vector frameMin, frameMax;
		m_MapEntity.GetMapVisibleFrame(frameMin, frameMax);
		const float frameX1 = frameMin[0];
		const float frameX2 = frameMax[0];
		const float frameY1 = frameMin[2];
		const float frameY2 = frameMax[2];
	
		foreach (GC_QuadNode node : m_aActiveNodes)
		{
			if (frameX2 > node.m_fX1 && frameX1 < node.m_fX2 && frameY2 > node.m_fY1 && frameY1 < node.m_fY2 && !node.m_bTransparentColor) // is the overhead worth it? not sure
			{
				// simply not recalculating them is insufficient, because it means they get stuck on the edge of the screen, they have to be removed as well
				
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
	}
	
	//! Still cheaper than 4 WorldToScreen calls :)
	void UpdateVerticesSingle(GC_QuadNode node)
	{
		if (!node.m_bTransparentColor)
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
	
	void SetResolutionMultiplier(float multiplier)
	{
		if (multiplier != m_fResolutionMultiplier)
		{
			m_fResolutionMultiplier = multiplier;
			m_bRestartScheduled = true;
		}
	}
	
	
	//! Maintains the quad tree, removing obsolete nodes and adding required nodes.
	protected void MaintainTree(bool restart)
	{
		
		if (restart)
		{
			Print(restart);
			m_NodeQueue.Clear();
			m_NodeQueue.Enqueue(m_QuadTree);
		}
		
		const int endTickCount = System.GetTickCount() + m_iTickBudget;
		
		int frameCost = 0;
		
		vector frameMin, frameMax;
		m_MapEntity.GetMapVisibleFrame(frameMin, frameMax);
		const float frameX1 = frameMin[0];
		const float frameX2 = frameMax[0];
		const float frameY1 = frameMin[2];
		const float frameY2 = frameMax[2];
		
		const float targetMeters = Math.Max(1, (frameX2 - frameX1) / (m_iQuadWidth * m_fResolutionMultiplier)); // e. g. 10m, no less than 1m
		const int intendedLevel = Math.Round(Math.Log2(m_MapEntity.GetMapSizeX() / targetMeters));
		// e. g. 4000m => 2000m => 1000m => 500m => 250m => 125m => 62.5m => 31.25m => 15.125m => 7m
		

		while (endTickCount > System.GetTickCount())
		{
			GC_QuadNode node = m_NodeQueue.Deque();
			if (!node)
				break; // done
			
			frameCost += 1;
			const bool levelReached = node.m_iLevel >= intendedLevel;
			const bool intersection = BboxIntersects(frameX1, frameX2, frameY1, frameY2, node.m_fX1, node.m_fX2, node.m_fY1, node.m_fY2);
			const bool hasChildren = node.m_Q1 != null;
			
			
			if (levelReached || !intersection)
			{
				// null + deactivate children, make self active (BUT ONLY SELF EVEN HAD ACTIVE CHILDREN)
				if (hasChildren)
				{
					if (DeactivateChildren(node))
						ActivateNode(node);
					node.m_Q1 = null;
					node.m_Q2 = null;
					node.m_Q3 = null;
					node.m_Q4 = null;
				}
			}
			else
			{
				if (!hasChildren)
				{
					float midX = node.m_fX1 + (node.m_fX2 - node.m_fX1) / 2;
					float midY = node.m_fY1 + (node.m_fY2 - node.m_fY1) / 2;
					
					//  II   I
					// III  IV
					
					node.m_Q1 = new GC_QuadNode(node, m_vSourcePos, m_fTargetOffset, node.m_iLevel + 1, midX, node.m_fX2, midY, node.m_fY2);
					node.m_Q2 = new GC_QuadNode(node, m_vSourcePos, m_fTargetOffset, node.m_iLevel + 1, node.m_fX1, midX, midY, node.m_fY2);
					node.m_Q3 = new GC_QuadNode(node, m_vSourcePos, m_fTargetOffset, node.m_iLevel + 1, node.m_fX1, midX, node.m_fY1, midY);
					node.m_Q4 = new GC_QuadNode(node, m_vSourcePos, m_fTargetOffset, node.m_iLevel + 1, midX, node.m_fX2, node.m_fY1, midY);
					
					// Check if any differences to current node
					const int eB = node.m_iEntsBlocked;
					const int tB = node.m_iTerrBlocked;
					const bool diff = 	node.m_Q1.m_iEntsBlocked != eB || node.m_Q2.m_iEntsBlocked != eB || node.m_Q3.m_iEntsBlocked != eB || node.m_Q4.m_iEntsBlocked != eB
									||	node.m_Q1.m_iTerrBlocked != tB || node.m_Q2.m_iTerrBlocked != tB || node.m_Q3.m_iTerrBlocked != tB || node.m_Q4.m_iTerrBlocked != tB;
					
					if (diff)
					{
						// upper siblings until active node is reached, then deactivate it
						GC_QuadNode n = node;
						while (n.m_iActiveIndex < 0)
						{
							ActivateSiblings(n);
							n = n.m_Parent;
						}
						DeactivateNode(n);
						
						// activate own children
						ActivateNode(node.m_Q1);
						ActivateNode(node.m_Q2);
						ActivateNode(node.m_Q3);
						ActivateNode(node.m_Q4);
					}
				}
				// enqueue children
				m_NodeQueue.Enqueue(node.m_Q1);
				m_NodeQueue.Enqueue(node.m_Q2);
				m_NodeQueue.Enqueue(node.m_Q3);
				m_NodeQueue.Enqueue(node.m_Q4);
			}
			
		}
		
		// only activate children with they disagree, else just enqueue them
		// if so, run activate method/loop, else do nothing
		//	which traverses upwards, activating siblings at each level until reaching an active node and deactivating it
		//	checking child disagreement is easy because you can just check against self (because if self is not the same as parent, self must be active anyway)
			
	}
	
	//! Traverses downwards, deactivating all children. Return whether it had any active children.
	bool DeactivateChildren(GC_QuadNode node)
	{
		bool hadActiveChildren = false;
		
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
			{
				hadActiveChildren = true;
				DeactivateNode(node);
			}
		}
		
		return hadActiveChildren;
	}

	void DeactivateNode(GC_QuadNode node)
	{
		const int activeIndex = node.m_iActiveIndex;
		if (activeIndex >= 0)
		{
			node.m_iActiveIndex = -1;
			m_aActiveNodes.Remove(activeIndex);
			if (activeIndex < m_aActiveNodes.Count())
				m_aActiveNodes[activeIndex].m_iActiveIndex = activeIndex;
		}
		
		const int commandIndex = node.m_iCommandIndex;
		if (commandIndex >= 0)
		{
			node.m_iCommandIndex = -1;
			m_aDrawCommands.Remove(commandIndex);
			if (commandIndex < m_aDrawCommands.Count())
				m_aActiveNodes[commandIndex].m_iCommandIndex = commandIndex;
		}
	}

	void ActivateNode(GC_QuadNode node)
	{
		if (node.m_iActiveIndex < 0) // this if is hopefully redundant 
		{
			node.m_iActiveIndex = m_aActiveNodes.Insert(node);
			
			if (!node.m_DrawCommand)
				node.CreateCommand();
			node.UpdateColor(m_bShadingMode);
			UpdateVerticesSingle(node);
			
			node.m_iCommandIndex = m_aDrawCommands.Insert(node.m_DrawCommand);
		}
	}
	
	//! Receives 2 bounding boxes and checks for intersection (2 must be greater than 1)
	static bool BboxIntersects(float ax1, float ax2, float ay1, float ay2, float bx1, float bx2, float by1, float by2)
	{
		return ax2 > bx1 && ax1 < bx2 && ay2 > by1 && ay1 < by2;
	}
	
	
	//! Activates all siblings, but not self
	protected void ActivateSiblings(GC_QuadNode node)
	{
		GC_QuadNode parent = node.m_Parent;
		if (parent.m_Q1 != node)
			ActivateNode(parent.m_Q1);
		if (parent.m_Q2 != node)
			ActivateNode(parent.m_Q2);
		if (parent.m_Q3 != node)
			ActivateNode(parent.m_Q3);
		if (parent.m_Q4 != node)
			ActivateNode(parent.m_Q4);
	}
	
	/// performance improvement avenues:
	
	//  - vectorize additional operations / move them out of functions into loops
	//  - a lot of performance cost comes from simply moving draw command vertices around
	//		i could try not drawing clearly off-screen things, and i could try prioritizing removal of active nodes over addition to keep the amount low when moving the map
	//		ccould also consider some way to just not process vertices of invisible nodes (i. e. 0 alpha)
	//  - zooming out from high res fast is currently still a big problem. there needs to be a way to deactivate these nodes earlier.
	
	
	// okay so
	// not drawing offscreen or transparent things requires adding some mode for it, other than active.
	// maybe a "visible" toggle on the node, which removes all vertices instead of calculating them.
	// if i wanted to remove the draw command entirely, i could no longer treat it as having the same index as in the active array, but i could just add another field in the node for it
	// reasons for nodes to be active but invisible: currently offscreen, transparent color / hidden
	// so node invisibility would be always determined when moving the map (vertex calculation), and also whenever the color is updated
	// node invisibility would be taken into account by updatevertices, which should only update if the node isn't invisible
	// also need to think about reinserting e. g. when hide status changes
	
	
	// make marker smaller
}