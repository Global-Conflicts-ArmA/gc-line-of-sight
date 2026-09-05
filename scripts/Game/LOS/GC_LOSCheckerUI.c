class GC_LOSCheckerUI : SCR_MapUIBaseComponent
{
	protected SCR_MapToolEntry m_ToolMenuEntry;
	protected SCR_MapCursorModule m_CursorModule;
	
	protected bool m_bIsActive = false;
	
	protected float m_fStartPointX, m_fStartPointY;
	
	protected Widget m_wLine;
	protected ImageWidget m_wLineImage;

	protected bool m_bActivationThrottle = true;

	protected ref array<ref CanvasWidgetCommand> m_aDrawCommands = {};
	protected CanvasWidget m_wCanvasWidget;
	protected ref array<GC_LOSCell> m_aCells = {};
	
	override void Init()
	{
		super.Init();
		
		Print("GC | Init");
		
		SCR_MapToolMenuUI toolMenu = SCR_MapToolMenuUI.Cast(m_MapEntity.GetMapUIComponent(SCR_MapToolMenuUI));
		if (!toolMenu)
			return;
		
		m_ToolMenuEntry = toolMenu.RegisterToolMenuEntry("{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset", "waypoint-once", 30);
		m_ToolMenuEntry.m_OnClick.Insert(Click);
		m_ToolMenuEntry.SetEnabled(true);
	}
	
	void Click()
	{
		ToggleActive();
	}
	
	protected void ToggleActive()
	{
		m_bIsActive = !m_bIsActive;
		
		if (m_bIsActive)
			SetLOSMode(true);
		else 
			SetLOSMode(false);
	}
	
	protected void SetLOSMode(bool state, bool cacheDrawn = false)
	{
		if (state)
		{
			GetGame().GetInputManager().AddActionListener("MapSelect", EActionTrigger.UP, OnMapClick);
			GetGame().GetInputManager().AddActionListener("MapContextualMenu", EActionTrigger.UP, OnMapContext);
			
			Widget mapFrame = m_MapEntity.GetMapMenuRoot().FindAnyWidget(SCR_MapConstants.MAP_FRAME_NAME);
			if (!mapFrame)
				mapFrame = m_MapEntity.GetMapMenuRoot();
			
			Widget w = GetGame().GetWorkspace().CreateWidgets("{F928661E727CC638}UI/Map/GC_LOSCanvas.layout", mapFrame);
			m_wCanvasWidget = CanvasWidget.Cast(w);
			
			m_bActivationThrottle = true;
		}
		else 
		{
			GetGame().GetInputManager().RemoveActionListener("MapSelect", EActionTrigger.UP, OnMapClick);
			GetGame().GetInputManager().RemoveActionListener("MapContextualMenu", EActionTrigger.UP, OnMapContext);

			m_wCanvasWidget = null;
			m_aDrawCommands = {};
			DestroyLine();
		}

		m_ToolMenuEntry.SetActive(state);
		m_bIsActive = state;
	}

	void ClearDrawing()
	{
		m_aDrawCommands.Clear();
	}
	
	void RegisterCells(array<GC_LOSCell> cells)
	{
		m_aCells = cells;
	}
	
	void DrawCells(array<GC_LOSCell> cells)
	{
		m_aDrawCommands.Clear();
		
		foreach(GC_LOSCell cell : cells)
		{
			Color color;
			if (!cell || cell.m_losResult == GC_SightTraceResult.Processing)
				continue;
			if (cell.m_losResult == GC_SightTraceResult.Free)
				color = Color.FromRGBA(0,255,0,63);
			else if (cell.m_losResult == GC_SightTraceResult.Terrain)
				color = Color.FromRGBA(255,0,0,63);
			else // view geo
				color = Color.FromRGBA(255,127,0,63);
			DrawShape(cell.m_vMin, cell.m_vMax, color);
		}
	}
	
	void DrawShape(vector worldPosition1, vector worldPosition2, Color color)
	{
		float screenX1, screenY1;
		float screenX2, screenY2;
		m_MapEntity.WorldToScreen(worldPosition1[0],worldPosition1[2],screenX1,screenY1, true);
		m_MapEntity.WorldToScreen(worldPosition2[0],worldPosition2[2],screenX2,screenY2, true);
		
		PolygonDrawCommand m_DrawCommand = new PolygonDrawCommand();
		m_DrawCommand.m_iColor = color.PackToInt();
		m_DrawCommand.m_Vertices = 
		{
			screenX1,screenY1,
			screenX2,screenY1,
			screenX2,screenY2,
			screenX1,screenY2
		};
		
		m_aDrawCommands.Insert(m_DrawCommand);

		m_wCanvasWidget.SetDrawCommands(m_aDrawCommands);
	}
	
		
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);
		
		m_CursorModule = SCR_MapCursorModule.Cast(m_MapEntity.GetMapModule(SCR_MapCursorModule));
	}
	
	override void OnMapClose(MapConfiguration config)
	{		
		SetLOSMode(false);
		
		super.OnMapClose(config);
	}
	
	protected void OnMapClick(float value, EActionTrigger reason)
	{
		if (m_bActivationThrottle)
		{
			m_bActivationThrottle = false;
			return;
		}

		CreateLine();
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_MAP_GADGET_MARKER_DRAW_START);
		
		float cursorX, cursorY;
		m_MapEntity.GetMapCursorWorldPosition(cursorX, cursorY);

		vector startPosition = Vector(cursorX, 0, cursorY);
		startPosition[1] = GetGame().GetWorld().GetSurfaceY(startPosition[0], startPosition[2]);
		
		ClearDrawing();
		GC_LOSSystem.GetInstance().SetStart(startPosition);
	}
	
	protected void OnMapContext(float value, EActionTrigger reason)
	{
		SetLOSMode(false);
	}
	
	void CreateLine()
	{
		Widget mapFrame = m_MapEntity.GetMapMenuRoot().FindAnyWidget(SCR_MapConstants.MAP_FRAME_NAME);
		if (!mapFrame)
			return;
		
		m_MapEntity.GetMapCursorWorldPosition(m_fStartPointX, m_fStartPointY);
		
		if (!m_wLine)
		{
			m_wLine = GetGame().GetWorkspace().CreateWidgets("{E8850FCD9219C411}UI/layouts/Map/MapDrawLine.layout", mapFrame);
			m_wLineImage = ImageWidget.Cast(m_wLine.FindAnyWidget("DrawLineImage"));
		}

		m_wLineImage.SetColorInt(Color.BLACK);
	}
	
	void DestroyLine()
	{
		if (m_wLine)
			m_wLine.RemoveFromHierarchy();
	}
	
	void UpdateLine()
	{
		if (!m_wLine)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;
		
		float endPointX, endPointY;
		m_MapEntity.GetMapCursorWorldPosition(endPointX, endPointY);
		int screenX, screenY, endX, endY;

		m_MapEntity.WorldToScreen(m_fStartPointX, m_fStartPointY, screenX, screenY, true);
		m_MapEntity.WorldToScreen(endPointX, endPointY, endX, endY, true);

		vector lineVector = vector.Zero;
		lineVector[0] = m_fStartPointX - endPointX;
		lineVector[1] = m_fStartPointY - endPointY;

		vector angles = lineVector.VectorToAngles();
		if (angles[0] == 90)
			angles[1] =  180 - angles[1];
		else if (angles[0] == 0)
			angles[1] =  180 + angles[1];

		m_wLineImage.SetRotation(angles[1]);

		lineVector = lineVector * m_MapEntity.GetCurrentZoom();

		float length = workspace.DPIUnscale(lineVector.Length());

		m_wLineImage.SetSize(length, 50);

		FrameSlot.SetPos(m_wLine, workspace.DPIUnscale(screenX), workspace.DPIUnscale(screenY));
	}
	
	override void Update(float timeSlice)
	{		
		if(!m_bIsActive || !m_wLine)
			return;

		UpdateLine();
		DrawCells(m_aCells);
		
		float cursorX, cursorY;
		m_MapEntity.GetMapCursorWorldPosition(cursorX, cursorY);
		GC_LOSSystem.GetInstance().UpdateAroundCursor(cursorX, cursorY);
	}
}
