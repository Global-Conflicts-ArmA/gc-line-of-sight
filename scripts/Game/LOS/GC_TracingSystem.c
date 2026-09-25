class GC_TracingSystem : GameSystem
{
	protected SCR_MapEntity m_MapEntity;
	
	//! Canvas the polygons are drawn onto
	protected CanvasWidget m_wCanvasWidget;
	
	//! Root node of quad tree
	protected ref GC_QuadNode m_QuadTree;
	
	//! Nodes eligible for drawing
	protected ref array<GC_QuadNode> m_aActiveNodes = {};
	
	//! Array of draw commands, first position reserved for source marker
	protected ref array<ref CanvasWidgetCommand> m_aDrawCommands = { null };
	
	//! Highlights current source position. Needs to be moved and updated.
	protected ref ImageDrawCommand m_SourceDrawing;
	
	//! Nodes still in queue for maintenance
	protected ref GC_SimpleQueue<GC_QuadNode> m_NodeQueue = new GC_SimpleQueue<GC_QuadNode>();
	
	//! Determines colorization of nodes
	protected GC_ShadingMode m_bShadingMode = GC_ShadingMode.Darken;
	
	//! Debug info widget, visible only in Workbench
	protected TextWidget m_wStatusWidget;

	//! For how many ms the system may trace per frame. Increases CPU load but speeds up subdivision.
	protected const int m_iTickBudget = 5;
	
	//! Target number for onscreen quads on the X axis
	protected const int m_iQuadWidth = 150;
	
	//! Adjustable multiplier for quad width
	protected float m_fResolutionMultiplier = 1;
	
	//! Source of trace
	protected vector m_vSourcePos;
	
	//! Destination Y offset for trace
	protected float m_fTargetOffset;
	
	//! Whether to discard current progress and restart maintenance of the tree
	protected bool m_bRestartScheduled;
	
	protected vector m_vPreviousPan;
	protected float m_fPreviousZoom;
	
	//! Remembers previous intended level for difference checking
	protected int m_iPreviousIntendedLevel;
	
	protected int m_iSourceDrawingSize;
	
	
	//! System setup
	override static void InitInfo(WorldSystemInfo outInfo)
	{
		super.InitInfo(outInfo);
		outInfo
			.SetAbstract(false)
			.SetUnique(true)
			.SetLocation(WorldSystemLocation.Client)
			.AddPoint(ESystemPoint.PostFrame);
	}
	
	//! System init
	override void OnInit()
	{
		super.OnInit();
		
		Enable(false);
		
		m_MapEntity = SCR_MapEntity.GetMapInstance();
	}
	
	//! Activate tracing system
	void ActivateTool(float worldX, float worldY, float sourceOffset, float targetOffset, TextWidget status)
	{
		DeactivateTool();
		#ifdef WORKBENCH
		Print("GC LOS | Activating tracing system");
		#endif
		
		// Set positions
		m_vSourcePos = Vector(worldX, Math.Max(0, GetGame().GetWorld().GetSurfaceY(worldX, worldY)) + sourceOffset, worldY);
		m_fTargetOffset = targetOffset;
		m_wStatusWidget = status;
		
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
		
		CreateSourceDrawing();
		
		Enable(true);
	}
	
	protected void CreateSourceDrawing()
	{
		m_iSourceDrawingSize = GetGame().GetWorkspace().GetWidth() / 100;
		m_SourceDrawing = m_wCanvasWidget.CreateCommandFromImageSet("{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset", "radialCircle", Vector(m_iSourceDrawingSize*2, m_iSourceDrawingSize*2, 0));
		m_SourceDrawing.m_iFlags |= WidgetFlags.BLEND;
		m_SourceDrawing.m_iColor = Color.ORANGE;
		m_aDrawCommands[0] = m_SourceDrawing;
		int x, y;
		m_MapEntity.WorldToScreen(m_vSourcePos[0], m_vSourcePos[2], x, y, true);
		m_SourceDrawing.m_Position = Vector(x - m_iSourceDrawingSize, y - m_iSourceDrawingSize, 0);
	}
	
	//! Deactivate tracing system
	void DeactivateTool()
	{
		#ifdef WORKBENCH
		Print("GC LOS | Deactivating tracing system");
		#endif
		
		if (m_wCanvasWidget)
   			 m_wCanvasWidget.RemoveFromHierarchy();
		
		m_NodeQueue.Clear();
		m_QuadTree = null;
		m_aActiveNodes.Clear();
		m_wCanvasWidget = null;
		m_aDrawCommands.Clear();
		m_aDrawCommands.Insert(null);
		m_SourceDrawing = null;
		
		m_vPreviousPan = vector.Zero;
		m_fPreviousZoom = 0;
		
		Enable(false);
	}
	
	//! Frame update event
	override void OnUpdate(WorldSystemPoint point)
	{
		super.OnUpdate(point);
		
		const vector currentPan = m_MapEntity.GetCurrentPan();
		const float currentZoom = m_MapEntity.GetCurrentZoom();
		vector frameMin, frameMax;
		m_MapEntity.GetMapVisibleFrame(frameMin, frameMax);
		
		const bool mapChange = (m_vPreviousPan != currentPan) || (m_fPreviousZoom != currentZoom);
		const int intendedLevel = CalculateIntendedLevel(frameMin[0], frameMax[0]);
		
		if (mapChange)
		{
			TryLevelReduction(intendedLevel);
			UpdateVerticesBulk();
		}
		else if (m_bRestartScheduled)
		{
			m_NodeQueue.Clear();
			m_NodeQueue.Enqueue(m_QuadTree);
			MaintainTree(intendedLevel);
		}
		else if (!m_NodeQueue.IsEmpty())
			MaintainTree(intendedLevel);
		
		m_vPreviousPan = currentPan;
		m_fPreviousZoom = currentZoom;
		m_bRestartScheduled = mapChange;
		m_iPreviousIntendedLevel = intendedLevel;

		#ifdef WORKBENCH
		m_wStatusWidget.SetText("Q: " + m_NodeQueue.Count() + " A: " + m_aActiveNodes.Count());
		#endif
		
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
			if (!node.m_bTransparentColor)
			{	
				const float x1 = (node.m_fX1 - offsetX) * zoom + panX;
				const float x2 = (node.m_fX2 - offsetX) * zoom + panX;
				const float y1 = (offsetY - node.m_fY1) * zoom + panY;
				const float y2 = (offsetY - node.m_fY2) * zoom + panY;
		
				array<float> verts = node.m_DrawCommand.m_Vertices;
				verts[0] = x2;
				verts[1] = y2;
				verts[2] = x1;
				verts[3] = y2;
				verts[4] = x1;
				verts[5] = y1;
				verts[6] = x2;
				verts[7] = y1;
			}
		}
		
		if (m_SourceDrawing)
		{
			m_SourceDrawing.m_Position[0] = (m_vSourcePos[0] - offsetX) * zoom + panX - m_iSourceDrawingSize;
			m_SourceDrawing.m_Position[1] = (offsetY - m_vSourcePos[2]) * zoom + panY - m_iSourceDrawingSize;
		}
	}
	// i just realized i can probably figure out one linear transformation and apply it to all vertices. but it's not a significant impact at this point.
	// if zoom did not change, the transformation is even just a simple addition, i. e. i can figure out a const x+y offset then simply do +=
	// problem is that you could lose accuracy eventually, so it would be problematic to do this all the time. will just leave it as is for now.
	
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
		
			array<float> verts = node.m_DrawCommand.m_Vertices;
			verts[0] = x2;
			verts[1] = y2;
			verts[2] = x1;
			verts[3] = y2;
			verts[4] = x1;
			verts[5] = y1;
			verts[6] = x2;
			verts[7] = y1;
		}
	}
	
	//! Check if level of active nodes needs to be reduced
	protected void TryLevelReduction(int level)
	{
		if (m_iPreviousIntendedLevel == level)
			return;
		
		const int intendedLevel = level; // make const
		
		if (m_iPreviousIntendedLevel > intendedLevel)
		{
			array<GC_QuadNode> parentsToActivate = {};
			foreach (GC_QuadNode node : m_aActiveNodes)
				if (node.m_iLevel > intendedLevel)
					parentsToActivate.Insert(node.m_Parent);
		
			foreach (GC_QuadNode parent : parentsToActivate)
			{
				if (parent.m_Q1) // parent was inserted 4 times, only do this once. not super elegant but works.
				{
					if (DeactivateChildren(parent))
						ActivateNode(parent);
					parent.m_Q1 = null;
					parent.m_Q2 = null;
					parent.m_Q3 = null;
					parent.m_Q4 = null;
				}
			}
		}
		
		m_iPreviousIntendedLevel = intendedLevel;
	}
	
	//! Change the shading mode
	void SetShadingMode(GC_ShadingMode mode)
	{
		if (m_bShadingMode != mode)
		{
			m_bShadingMode = mode;
			foreach (GC_QuadNode node : m_aActiveNodes)
				node.UpdateColor(m_bShadingMode);
		}
	}
	
	//! Change the resolution multiplier
	void SetResolutionMultiplier(float multiplier)
	{
		if (multiplier != m_fResolutionMultiplier)
		{
			m_fResolutionMultiplier = multiplier;
			m_bRestartScheduled = true;
		}
	}
		
	//! Determine intended tree depth within view
	protected int CalculateIntendedLevel(int frameX1, int frameX2)
	{
		return Math.Round(Math.Log2(m_MapEntity.GetMapSizeX() / Math.Max(1, (frameX2 - frameX1) / (m_iQuadWidth * m_fResolutionMultiplier))));
	}
	
	
	//! Maintains the quad tree, removing+deactivating obsolete nodes and creating+activating new nodes
	protected void MaintainTree(int level)
	{
		const int endTickCount = System.GetTickCount() + m_iTickBudget;
		
		vector frameMin, frameMax;
		m_MapEntity.GetMapVisibleFrame(frameMin, frameMax);
		const float frameX1 = frameMin[0];
		const float frameX2 = frameMax[0];
		const float frameY1 = frameMin[2];
		const float frameY2 = frameMax[2];
		
		const int intendedLevel = level; // make const

		while (endTickCount > System.GetTickCount())
		{
			GC_QuadNode node = m_NodeQueue.Deque();
			if (!node)
				break; // done
			
			const bool levelReached = node.m_iLevel >= intendedLevel;
			const bool intersection = BboxIntersects(frameX1, frameX2, frameY1, frameY2, node.m_fX1, node.m_fX2, node.m_fY1, node.m_fY2);
			const bool hasChildren = node.m_Q1;
			
			if (levelReached || !intersection)
			{
				// null + deactivate children, make self active if deleted children were active
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

	//! Remove node from m_aActiveNodes and its draw command from m_aDrawCommands
	void DeactivateNode(GC_QuadNode node)
	{
		const int index = node.m_iActiveIndex;
		if (index >= 0)
		{
			node.m_iActiveIndex = -1;
			m_aActiveNodes.Remove(index);
			m_aDrawCommands.Remove(index + 1);
			if (index < m_aActiveNodes.Count())
				m_aActiveNodes[index].m_iActiveIndex = index;
		}
	}

	//! Add node to m_aActiveNodes and its draw command to m_aDrawCommands
	void ActivateNode(GC_QuadNode node)
	{
		if (node.m_iActiveIndex < 0) // this if is hopefully redundant 
		{
			node.m_iActiveIndex = m_aActiveNodes.Insert(node);
			
			if (!node.m_DrawCommand)
				node.CreateCommand();
			node.UpdateColor(m_bShadingMode);
			m_aDrawCommands.Insert(node.m_DrawCommand);
			UpdateVerticesSingle(node);
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
}